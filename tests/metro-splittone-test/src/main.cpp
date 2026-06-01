// Copyright (c) 2026 Metro Design. All rights reserved.
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <cstdlib>

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

#define CHECK(cond) do { \
    if (!(cond)) { std::printf("FAIL at %s:%d\n", __FILE__, __LINE__); std::exit(1); } \
} while(0)

static float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

static float hueToRgb(float p, float q, float t)
{
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

static void rgbToHsl(float r, float g, float b, float &h, float &s, float &l)
{
    float mx = std::max({r, g, b});
    float mn = std::min({r, g, b});
    l = (mx + mn) * 0.5f;
    if (mx == mn) { h = 0.0f; s = 0.0f; return; }
    float d = mx - mn;
    s = (l > 0.5f) ? d / (2.0f - mx - mn) : d / (mx + mn);
    if (mx == r)      h = (g - b) / d + (g < b ? 6.0f : 0.0f);
    else if (mx == g) h = (b - r) / d + 2.0f;
    else              h = (r - g) / d + 4.0f;
    h /= 6.0f;
}

static void hslToRgb(float h, float s, float l, float &r, float &g, float &b)
{
    if (s == 0.0f) { r = g = b = l; return; }
    float q = (l < 0.5f) ? l * (1.0f + s) : l + s - l * s;
    float p = 2.0f * l - q;
    r = hueToRgb(p, q, h + 1.0f / 3.0f);
    g = hueToRgb(p, q, h);
    b = hueToRgb(p, q, h - 1.0f / 3.0f);
}

static float shortestHueDelta(float a, float b)
{
    float d = b - a;
    if (d > 0.5f) d -= 1.0f;
    if (d < -0.5f) d += 1.0f;
    return d;
}

static float smoothEdge(float x)
{
    return x * x * (3.0f - 2.0f * x);
}

static float computeBlendWeight(float luma, float balance)
{
    float pivot = 0.5f - balance * 0.4f;
    pivot = clampf(pivot, 0.1f, 0.9f);
    float softness = 0.25f;
    float t = (luma - pivot + softness) / (2.0f * softness);
    t = clampf(t, 0.0f, 1.0f);
    return smoothEdge(t);
}

static float rec709Luma(float r, float g, float b)
{
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

static float dE76(float r1, float g1, float b1, float r2, float g2, float b2)
{
    float dr = r1 - r2;
    float dg = g1 - g2;
    float db = b1 - b2;
    return std::sqrt(dr * dr + dg * dg + db * db);
}

static bool approxEq(float a, float b, float eps = 1.0e-6f)
{
    return std::fabs(a - b) <= eps;
}

void test_rgb_to_hsl_known_colors()
{
    TEST("RGB->HSL: red (1,0,0)");
    float h, s, l;
    rgbToHsl(1.0f, 0.0f, 0.0f, h, s, l);
    CHECK(approxEq(h, 0.0f));
    CHECK(approxEq(s, 1.0f));
    CHECK(approxEq(l, 0.5f));
    PASS();
}

void test_rgb_to_hsl_green()
{
    TEST("RGB->HSL: green (0,1,0)");
    float h, s, l;
    rgbToHsl(0.0f, 1.0f, 0.0f, h, s, l);
    CHECK(approxEq(h, 1.0f / 3.0f));
    CHECK(approxEq(s, 1.0f));
    CHECK(approxEq(l, 0.5f));
    PASS();
}

void test_rgb_to_hsl_blue()
{
    TEST("RGB->HSL: blue (0,0,1)");
    float h, s, l;
    rgbToHsl(0.0f, 0.0f, 1.0f, h, s, l);
    CHECK(approxEq(h, 2.0f / 3.0f));
    CHECK(approxEq(s, 1.0f));
    CHECK(approxEq(l, 0.5f));
    PASS();
}

void test_rgb_to_hsl_black()
{
    TEST("RGB->HSL: black (0,0,0)");
    float h, s, l;
    rgbToHsl(0.0f, 0.0f, 0.0f, h, s, l);
    CHECK(approxEq(l, 0.0f));
    CHECK(s == 0.0f);
    PASS();
}

void test_rgb_to_hsl_white()
{
    TEST("RGB->HSL: white (1,1,1)");
    float h, s, l;
    rgbToHsl(1.0f, 1.0f, 1.0f, h, s, l);
    CHECK(approxEq(l, 1.0f));
    CHECK(s == 0.0f);
    PASS();
}

void test_hsl_roundtrip()
{
    TEST("HSL round-trip: 11 known colors dE76 < 1e-6");
    const float testColors[][3] = {
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f},
        {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
        {0.2f, 0.4f, 0.6f}, {0.8f, 0.1f, 0.3f},
    };
    for (auto &c : testColors) {
        float h, s, l, r2, g2, b2;
        rgbToHsl(c[0], c[1], c[2], h, s, l);
        hslToRgb(h, s, l, r2, g2, b2);
        CHECK(dE76(c[0], c[1], c[2], r2, g2, b2) <= 1.0e-6f);
    }
    PASS();
}

void test_rec709_luma()
{
    TEST("rec709Luma: black");
    CHECK(rec709Luma(0.0f, 0.0f, 0.0f) == 0.0f);
    PASS();

    TEST("rec709Luma: white");
    CHECK(approxEq(rec709Luma(1.0f, 1.0f, 1.0f), 1.0f));
    PASS();

    TEST("rec709Luma: neutral gray");
    CHECK(approxEq(rec709Luma(0.5f, 0.5f, 0.5f), 0.5f));
    PASS();

    TEST("rec709Luma: green > red > blue");
    CHECK(rec709Luma(1.0f, 0.0f, 0.0f) < rec709Luma(0.0f, 1.0f, 0.0f));
    CHECK(rec709Luma(0.0f, 0.0f, 1.0f) < rec709Luma(1.0f, 0.0f, 0.0f));
    PASS();

    TEST("rec709Luma: coefficients sum to 1");
    CHECK(approxEq(
        rec709Luma(1.0f, 0.0f, 0.0f) + rec709Luma(0.0f, 1.0f, 0.0f) + rec709Luma(0.0f, 0.0f, 1.0f),
        1.0f));
    PASS();
}

void test_shortest_hue_delta()
{
    TEST("shortestHueDelta: zero delta");
    CHECK(shortestHueDelta(0.0f, 0.0f) == 0.0f);
    PASS();

    TEST("shortestHueDelta: forward 0.2");
    CHECK(approxEq(shortestHueDelta(0.1f, 0.3f), 0.2f));
    PASS();

    TEST("shortestHueDelta: backward -0.2");
    CHECK(approxEq(shortestHueDelta(0.3f, 0.1f), -0.2f));
    PASS();

    TEST("shortestHueDelta: wrap forward");
    CHECK(approxEq(shortestHueDelta(0.9f, 0.1f), 0.2f));
    PASS();

    TEST("shortestHueDelta: wrap backward");
    CHECK(approxEq(shortestHueDelta(0.1f, 0.9f), -0.2f));
    PASS();
}

void test_compute_blend_weight()
{
    TEST("computeBlendWeight: black => w=0");
    CHECK(computeBlendWeight(0.0f, 0.0f) == 0.0f);
    PASS();

    TEST("computeBlendWeight: white => w=1");
    CHECK(computeBlendWeight(1.0f, 0.0f) == 1.0f);
    PASS();

    TEST("computeBlendWeight: mid-gray ~0.5");
    CHECK(computeBlendWeight(0.5f, 0.0f) > 0.4f);
    CHECK(computeBlendWeight(0.5f, 0.0f) < 0.6f);
    PASS();

    TEST("computeBlendWeight: neg balance shifts pivot");
    CHECK(computeBlendWeight(0.5f, -1.0f) < computeBlendWeight(0.5f, 1.0f));
    PASS();

    TEST("computeBlendWeight: monotonic with luma");
    for (int i = 0; i < 100; ++i) {
        float l0 = static_cast<float>(i) / 100.0f;
        float l1 = static_cast<float>(i + 1) / 100.0f;
        CHECK(computeBlendWeight(l0, 0.0f) <= computeBlendWeight(l1, 0.0f));
    }
    PASS();
}

void test_smooth_edge_properties()
{
    TEST("smoothEdge(0) == 0");
    CHECK(smoothEdge(0.0f) == 0.0f);
    PASS();

    TEST("smoothEdge(1) == 1");
    CHECK(smoothEdge(1.0f) == 1.0f);
    PASS();

    TEST("smoothEdge(0.5) == 0.5");
    CHECK(approxEq(smoothEdge(0.5f), 0.5f));
    PASS();

    TEST("smoothEdge: symmetric");
    CHECK(approxEq(smoothEdge(0.25f), 1.0f - smoothEdge(0.75f)));
    PASS();
}

int main()
{
    std::printf("metro-splittone-test: Split Tone Plugin Unit Tests\n");
    std::printf("===================================================\n\n");

    test_rgb_to_hsl_known_colors();
    test_rgb_to_hsl_green();
    test_rgb_to_hsl_blue();
    test_rgb_to_hsl_black();
    test_rgb_to_hsl_white();
    test_hsl_roundtrip();
    test_rec709_luma();
    test_shortest_hue_delta();
    test_compute_blend_weight();
    test_smooth_edge_properties();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);

    return (s_passCount == s_testCount) ? 0 : 1;
}
