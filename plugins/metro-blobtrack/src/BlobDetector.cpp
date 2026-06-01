// Copyright (c) 2026 Metro Design. All rights reserved.
#include "metro/blobtrack/BlobDetector.hpp"

#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstddef>
#include <limits>
#include <utility>

namespace metro::blobtrack {

BlobDetector::BlobDetector() = default;
BlobDetector::~BlobDetector() = default;

namespace {

struct UnionFind {
    std::vector<int> parent;
    explicit UnionFind(int n) : parent(n) {
        for (int i = 0; i < n; ++i) parent[i] = i;
    }
    int find(int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }
    void unite(int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra != rb) parent[rb] = ra;
    }
};

struct LabelStats {
    int   area{0};
    double sumX{0.0}, sumY{0.0};
    double sumIntensity{0.0};
    int   minX{std::numeric_limits<int>::max()};
    int   minY{std::numeric_limits<int>::max()};
    int   maxX{0};
    int   maxY{0};
};

} // anonymous namespace

std::vector<Blob> BlobDetector::detect(const uint8_t *gray, int width, int height,
                                        int stride, const BlobParams &params)
{
    if (!gray || width <= 0 || height <= 0)
        return {};

    const float thresh = std::max(0.001f, std::min(0.999f, params.threshold));
    const int rawThreshold = static_cast<int>(thresh * 255.0f);

    std::vector<int> labels(static_cast<size_t>(width) * height, 0);
    int nextLabel = 0;
    UnionFind uf(width * height / 4 + 1);

    // Pass 1: raster scan with provisional labeling
    for (int y = 0; y < height; ++y) {
        const uint8_t *row = gray + y * stride;
        int *labelRow = labels.data() + y * width;
        const int *labelPrev = (y > 0) ? (labelRow - width) : nullptr;

        for (int x = 0; x < width; ++x) {
            if (row[x] < rawThreshold) {
                labelRow[x] = 0;
                continue;
            }

            // Check 4-connected neighbors (left and above)
            int leftLabel = (x > 0) ? labelRow[x - 1] : 0;
            int aboveLabel = (labelPrev) ? labelPrev[x] : 0;

            if (leftLabel == 0 && aboveLabel == 0) {
                labelRow[x] = ++nextLabel;
            } else if (leftLabel != 0 && aboveLabel == 0) {
                labelRow[x] = leftLabel;
            } else if (leftLabel == 0 && aboveLabel != 0) {
                labelRow[x] = aboveLabel;
            } else {
                // Both neighbors have labels — use smallest, union them
                int minL = std::min(leftLabel, aboveLabel);
                labelRow[x] = minL;
                if (leftLabel != aboveLabel) {
                    uf.unite(leftLabel, aboveLabel);
                }
            }
        }
    }

    if (nextLabel == 0)
        return {};

    // Pass 2: resolve labels and accumulate statistics
    std::vector<LabelStats> stats(static_cast<size_t>(nextLabel) + 1);

    for (int y = 0; y < height; ++y) {
        const uint8_t *row = gray + y * stride;
        int *labelRow = labels.data() + y * width;
        for (int x = 0; x < width; ++x) {
            int l = labelRow[x];
            if (l == 0) continue;
            int root = uf.find(l);
            labelRow[x] = root;
            auto &s = stats[root];
            s.area++;
            s.sumX += static_cast<double>(x);
            s.sumY += static_cast<double>(y);
            s.sumIntensity += static_cast<double>(row[x]);
            if (x < s.minX) s.minX = x;
            if (y < s.minY) s.minY = y;
            if (x > s.maxX) s.maxX = x;
            if (y > s.maxY) s.maxY = y;
        }
    }

    // Collect blobs, filter by size
    std::vector<Blob> blobs;
    int blobId = 0;
    for (int i = 1; i <= nextLabel; ++i) {
        if (stats[i].area == 0) continue;
        if (stats[i].area < params.minBlobArea) continue;
        if (stats[i].area > params.maxBlobArea) continue;

        Blob b;
        b.id = ++blobId;
        b.label = i;
        b.area = stats[i].area;
        b.centroidX = static_cast<float>(stats[i].sumX / stats[i].area);
        b.centroidY = static_cast<float>(stats[i].sumY / stats[i].area);
        b.minX = stats[i].minX;
        b.minY = stats[i].minY;
        b.maxX = stats[i].maxX;
        b.maxY = stats[i].maxY;
        b.meanIntensity = static_cast<float>(stats[i].sumIntensity / stats[i].area);
        blobs.push_back(b);
    }

    // Proximity merge: merge blobs whose centroids are within mergeDist
    if (params.proximityMerge > 0.f && blobs.size() > 1u) {
        const float mergeDistSq = params.proximityMerge * params.proximityMerge;
        bool merged = true;
        while (merged) {
            merged = false;
            for (size_t i = 0; i < blobs.size(); ++i) {
                for (size_t j = i + 1; j < blobs.size(); ++j) {
                    float dx = blobs[i].centroidX - blobs[j].centroidX;
                    float dy = blobs[i].centroidY - blobs[j].centroidY;
                    if (dx * dx + dy * dy <= mergeDistSq) {
                        // Merge j into i
                        int a1 = blobs[i].area;
                        int a2 = blobs[j].area;
                        int totalArea = a1 + a2;
                        blobs[i].centroidX = (blobs[i].centroidX * a1 + blobs[j].centroidX * a2) / totalArea;
                        blobs[i].centroidY = (blobs[i].centroidY * a1 + blobs[j].centroidY * a2) / totalArea;
                        blobs[i].area = totalArea;
                        blobs[i].minX = std::min(blobs[i].minX, blobs[j].minX);
                        blobs[i].minY = std::min(blobs[i].minY, blobs[j].minY);
                        blobs[i].maxX = std::max(blobs[i].maxX, blobs[j].maxX);
                        blobs[i].maxY = std::max(blobs[i].maxY, blobs[j].maxY);
                        blobs[i].meanIntensity = (blobs[i].meanIntensity * a1 + blobs[j].meanIntensity * a2) / totalArea;
                        blobs.erase(blobs.begin() + static_cast<std::ptrdiff_t>(j));
                        merged = true;
                        break;
                    }
                }
                if (merged) break;
            }
        }
        // Reassign IDs
        for (size_t i = 0; i < blobs.size(); ++i)
            blobs[i].id = static_cast<int>(i + 1);
    }

    return blobs;
}

std::vector<TrackedBlob> BlobDetector::track(const std::vector<Blob> &blobs)
{
    std::vector<TrackedBlob> result;

    if (blobs.empty()) {
        prevBlobs_.clear();
        return result;
    }

    // Match new blobs to previous tracked blobs via nearest-neighbor
    std::vector<bool> matched(blobs.size(), false);
    std::vector<bool> prevMatched(prevBlobs_.size(), false);

    const float maxMatchDist = 100.f;
    const float maxMatchDistSq = maxMatchDist * maxMatchDist;

    // For each new blob, find the closest previous tracked blob
    for (size_t i = 0; i < blobs.size(); ++i) {
        float bestDistSq = maxMatchDistSq;
        int bestPrev = -1;

        for (size_t j = 0; j < prevBlobs_.size(); ++j) {
            if (prevMatched[j]) continue;
            float dx = blobs[i].centroidX - prevBlobs_[j].centroidX;
            float dy = blobs[i].centroidY - prevBlobs_[j].centroidY;
            float d2 = dx * dx + dy * dy;
            if (d2 < bestDistSq) {
                bestDistSq = d2;
                bestPrev = static_cast<int>(j);
            }
        }

        if (bestPrev >= 0) {
            matched[i] = true;
            prevMatched[bestPrev] = true;
        }
    }

    // Build result
    for (size_t i = 0; i < blobs.size(); ++i) {
        TrackedBlob tb;
        tb.centroidX = blobs[i].centroidX;
        tb.centroidY = blobs[i].centroidY;
        tb.area = blobs[i].area;
        tb.minX = blobs[i].minX;
        tb.minY = blobs[i].minY;
        tb.maxX = blobs[i].maxX;
        tb.maxY = blobs[i].maxY;

        if (matched[i]) {
            // Find which prev blob it matched
            for (size_t j = 0; j < prevBlobs_.size(); ++j) {
                if (prevMatched[j] && std::abs(prevBlobs_[j].centroidX - tb.centroidX) < maxMatchDist &&
                    std::abs(prevBlobs_[j].centroidY - tb.centroidY) < maxMatchDist) {
                    float dx = blobs[i].centroidX - prevBlobs_[j].centroidX;
                    float dy = blobs[i].centroidY - prevBlobs_[j].centroidY;
                    if (dx * dx + dy * dy < maxMatchDistSq) {
                        tb.id = prevBlobs_[j].id;
                        // Copy trail and advance
                        int tl = prevBlobs_[j].trailLen;
                        int cap = TrackedBlob::kMaxTrail;
                        int copyCount = (tl < cap - 1) ? tl : cap - 1;
                        int srcStart = (prevBlobs_[j].trailHead - copyCount + cap) % cap;
                        tb.trailHead = 0;
                        for (int k = 0; k < copyCount; ++k) {
                            int srcIdx = (srcStart + k) % cap;
                            tb.trailX[k] = prevBlobs_[j].trailX[srcIdx];
                            tb.trailY[k] = prevBlobs_[j].trailY[srcIdx];
                            tb.trailHead++;
                        }
                        // Append current centroid
                        tb.trailX[tb.trailHead] = tb.centroidX;
                        tb.trailY[tb.trailHead] = tb.centroidY;
                        tb.trailHead = (tb.trailHead + 1) % cap;
                        tb.trailLen = std::min(cap, copyCount + 1);
                        break;
                    }
                }
            }
        }

        if (tb.id == 0) {
            tb.id = nextTrackId();
            tb.trailX[0] = tb.centroidX;
            tb.trailY[0] = tb.centroidY;
            tb.trailHead = 1;
            tb.trailLen = 1;
        }

        result.push_back(tb);
    }

    prevBlobs_ = result;
    return result;
}

void BlobDetector::resetTracking()
{
    prevBlobs_.clear();
    nextId_ = 0;
}

} // namespace metro::blobtrack
