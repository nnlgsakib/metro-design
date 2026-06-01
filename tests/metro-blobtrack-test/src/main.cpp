#include <cstdio>
#include <cassert>
#include <cstring>
#include <cmath>

#include "metro/blobtrack/BlobDetector.hpp"

using namespace metro::blobtrack;

static int s_testCount = 0;
static int s_passCount = 0;

#define TEST(name) do { \
    s_testCount++; \
    std::printf("  TEST: %s ... ", name); \
} while(0)

#define PASS() do { \
    s_passCount++; \
    std::printf("PASS\n"); \
} while(0)

#define FAIL(msg) do { \
    std::printf("FAIL: %s\n", msg); \
} while(0)

static bool feq(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) < eps;
}

void test_blob_default_construction()
{
    TEST("Blob default construction");

    Blob b;
    assert(b.id == 0);
    assert(b.label == 0);
    assert(b.area == 0);
    assert(feq(b.centroidX, 0.0f));
    assert(feq(b.centroidY, 0.0f));
    assert(b.minX == 0);
    assert(b.minY == 0);
    assert(b.maxX == 0);
    assert(b.maxY == 0);
    assert(feq(b.meanIntensity, 0.0f));

    PASS();
}

void test_params_default_construction()
{
    TEST("BlobParams default construction");

    BlobParams p;
    assert(feq(p.threshold, 0.5f));
    assert(p.minBlobArea == 10);
    assert(p.maxBlobArea == 5000);
    assert(feq(p.proximityMerge, 10.0f));
    assert(p.enabled == true);

    PASS();
}

void test_detect_empty_frame()
{
    TEST("detect on zero-size frame returns empty");

    BlobDetector bd;
    BlobParams p;
    auto blobs = bd.detect(nullptr, 0, 0, 0, p);
    assert(blobs.empty());

    PASS();
}

void test_detect_all_black()
{
    TEST("detect on all-black frame returns empty");

    const int w = 16, h = 16;
    uint8_t frame[w * h];
    std::memset(frame, 0, sizeof(frame));

    BlobDetector bd;
    BlobParams p;
    p.threshold = 0.5f;

    auto blobs = bd.detect(frame, w, h, w, p);
    assert(blobs.empty());

    PASS();
}

void test_detect_single_blob()
{
    TEST("detect finds a single white blob on black background");

    const int w = 32, h = 32;
    uint8_t frame[w * h];
    std::memset(frame, 0, sizeof(frame));

    // Draw a 4x4 white square in the center
    for (int y = 14; y < 18; ++y)
        for (int x = 14; x < 18; ++x)
            frame[y * w + x] = 200;

    BlobDetector bd;
    BlobParams p;
    p.threshold = 0.3f;
    p.minBlobArea = 1;

    auto blobs = bd.detect(frame, w, h, w, p);
    assert(blobs.size() == 1);
    assert(blobs[0].area == 16);
    assert(feq(blobs[0].centroidX, 15.5f));
    assert(feq(blobs[0].centroidY, 15.5f));
    assert(blobs[0].minX == 14);
    assert(blobs[0].minY == 14);
    assert(blobs[0].maxX == 17);
    assert(blobs[0].maxY == 17);

    PASS();
}

void test_detect_two_disjoint_blobs()
{
    TEST("detect finds two separate blobs");

    const int w = 32, h = 32;
    uint8_t frame[w * h];
    std::memset(frame, 0, sizeof(frame));

    // Blob 1 at top-left
    for (int y = 2; y < 6; ++y)
        for (int x = 2; x < 6; ++x)
            frame[y * w + x] = 200;

    // Blob 2 at bottom-right
    for (int y = 24; y < 28; ++y)
        for (int x = 24; x < 28; ++x)
            frame[y * w + x] = 200;

    BlobDetector bd;
    BlobParams p;
    p.threshold = 0.3f;
    p.minBlobArea = 1;
    p.proximityMerge = 0; // No merging

    auto blobs = bd.detect(frame, w, h, w, p);
    assert(blobs.size() == 2);
    assert(blobs[0].area == 16);
    assert(blobs[1].area == 16);

    PASS();
}

void test_detect_area_filter()
{
    TEST("detect area filter removes small blobs");

    const int w = 32, h = 32;
    uint8_t frame[w * h];
    std::memset(frame, 0, sizeof(frame));

    // Small blob (2x2 = 4 pixels)
    for (int y = 2; y < 4; ++y)
        for (int x = 2; x < 4; ++x)
            frame[y * w + x] = 200;

    // Large blob (6x6 = 36 pixels)
    for (int y = 20; y < 26; ++y)
        for (int x = 20; x < 26; ++x)
            frame[y * w + x] = 200;

    BlobDetector bd;
    BlobParams p;
    p.threshold = 0.3f;
    p.minBlobArea = 10;
    p.maxBlobArea = 50;

    auto blobs = bd.detect(frame, w, h, w, p);
    assert(blobs.size() == 1);
    assert(blobs[0].area == 36);

    PASS();
}

void test_proximity_merge()
{
    TEST("proximity merge combines nearby blobs");

    const int w = 32, h = 32;
    uint8_t frame[w * h];
    std::memset(frame, 0, sizeof(frame));

    // Two blobs close together (within merge distance of 8)
    // Blob 1 at (10..13), centroid ~11.5
    for (int y = 10; y < 14; ++y)
        for (int x = 10; x < 14; ++x)
            frame[y * w + x] = 200;
    // Blob 2 at (14..17), centroid ~15.5 — just 4 pixels away
    for (int y = 10; y < 14; ++y)
        for (int x = 14; x < 18; ++x)
            frame[y * w + x] = 200;

    BlobDetector bd;
    BlobParams p;
    p.threshold = 0.3f;
    p.minBlobArea = 1;
    p.proximityMerge = 6.0f;  // centroids ~4 apart, so they merge

    auto blobs = bd.detect(frame, w, h, w, p);
    // Two blobs with centroids 5 apart should merge
    assert(blobs.size() == 1);
    assert(blobs[0].area == 32);

    PASS();
}

void test_track_empty()
{
    TEST("track on empty blobs returns empty");

    BlobDetector bd;
    std::vector<Blob> empty;
    auto tracked = bd.track(empty);
    assert(tracked.empty());

    PASS();
}

void test_track_assigns_ids()
{
    TEST("track assigns sequential IDs to new blobs");

    BlobDetector bd;

    std::vector<Blob> blobs;
    Blob b1; b1.centroidX = 10.f; b1.centroidY = 10.f; b1.area = 16;
    Blob b2; b2.centroidX = 50.f; b2.centroidY = 50.f; b2.area = 25;
    blobs.push_back(b1);
    blobs.push_back(b2);

    auto tracked = bd.track(blobs);
    assert(tracked.size() == 2);
    assert(tracked[0].id == 1);
    assert(tracked[1].id == 2);
    assert(feq(tracked[0].centroidX, 10.f));
    assert(feq(tracked[1].centroidX, 50.f));

    PASS();
}

void test_track_persistence()
{
    TEST("track maintains same ID for persistent blob");

    BlobDetector bd;

    // Frame 1
    std::vector<Blob> blobs1;
    Blob b1; b1.centroidX = 10.f; b1.centroidY = 10.f; b1.area = 16;
    blobs1.push_back(b1);
    auto t1 = bd.track(blobs1);
    assert(t1.size() == 1);
    int id = t1[0].id;
    assert(id > 0);

    // Frame 2 - same position
    std::vector<Blob> blobs2;
    Blob b2; b2.centroidX = 12.f; b2.centroidY = 11.f; b2.area = 16;
    blobs2.push_back(b2);
    auto t2 = bd.track(blobs2);
    assert(t2.size() == 1);
    assert(t2[0].id == id);
    assert(t2[0].trailLen >= 2);

    PASS();
}

void test_reset_tracking()
{
    TEST("resetTracking clears state and IDs restart");

    BlobDetector bd;

    std::vector<Blob> blobs;
    Blob b; b.centroidX = 10.f; b.centroidY = 10.f; b.area = 16;
    blobs.push_back(b);

    auto t1 = bd.track(blobs);
    assert(!t1.empty());
    int id1 = t1[0].id;

    bd.resetTracking();

    auto t2 = bd.track(blobs);
    assert(!t2.empty());
    assert(t2[0].id != id1);

    PASS();
}

void test_detect_matches_params()
{
    TEST("detect parameters affect output correctly");

    const int w = 32, h = 32;
    uint8_t frame[w * h];
    std::memset(frame, 0, sizeof(frame));
    for (int y = 10; y < 20; ++y)
        for (int x = 10; x < 20; ++x)
            frame[y * w + x] = 200;

    BlobDetector bd;

    // High threshold = no blobs
    BlobParams highThresh;
    highThresh.threshold = 0.9f;
    highThresh.minBlobArea = 1;
    auto blobsHigh = bd.detect(frame, w, h, w, highThresh);
    assert(blobsHigh.empty());

    // Low threshold = finds blob
    BlobParams lowThresh;
    lowThresh.threshold = 0.3f;
    lowThresh.minBlobArea = 1;
    auto blobsLow = bd.detect(frame, w, h, w, lowThresh);
    assert(!blobsLow.empty());

    // minBlobArea too high = no blobs
    BlobParams minHigh;
    minHigh.threshold = 0.3f;
    minHigh.minBlobArea = 10000;
    auto blobsMin = bd.detect(frame, w, h, w, minHigh);
    assert(blobsMin.empty());

    PASS();
}

void test_tracked_blob_defaults()
{
    TEST("TrackedBlob default construction");

    TrackedBlob tb;
    assert(tb.id == 0);
    assert(feq(tb.centroidX, 0.f));
    assert(feq(tb.centroidY, 0.f));
    assert(tb.area == 0);
    assert(tb.trailLen == 0);
    assert(tb.trailHead == 0);

    PASS();
}

int main()
{
    std::printf("metro-blobtrack-test: Blob Detection & Tracking Unit Tests\n");
    std::printf("==========================================================\n\n");

    test_blob_default_construction();
    test_params_default_construction();
    test_detect_empty_frame();
    test_detect_all_black();
    test_detect_single_blob();
    test_detect_two_disjoint_blobs();
    test_detect_area_filter();
    test_proximity_merge();
    test_track_empty();
    test_track_assigns_ids();
    test_track_persistence();
    test_reset_tracking();
    test_detect_matches_params();
    test_tracked_blob_defaults();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);

    return (s_passCount == s_testCount) ? 0 : 1;
}
