#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>

static int s_testCount = 0;
static int s_passCount = 0;

#define TEST(name) do { \
    s_testCount++; \
    std::printf("  TEST: %s ... ", name); \
} while(0)

#define PASS() do { s_passCount++; std::printf("PASS\n"); } while(0)
#define FAIL(msg) do { std::printf("FAIL at %s:%d\n  %s\n", __FILE__, __LINE__, msg); return; } while(0)
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL at %s:%d\n  '%s' is false\n", __FILE__, __LINE__, #cond); return; } } while(0)
#define CHECK_NEAR(a, b, eps) do { \
    float _a = (a), _b = (b); \
    if (std::fabs(_a - _b) > (eps)) { \
        std::printf("FAIL at %s:%d\n  '%s' = %f != %f = '%s' (eps=%f)\n", \
            __FILE__, __LINE__, #a, _a, _b, #b, (float)(eps)); \
        return; \
    } \
} while(0)

// ---- Helpers (duplicated from plugin for isolated test) ----

static float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

static unsigned int hashUIntCPU(unsigned int x, unsigned int y, unsigned int seed)
{
    unsigned int h = x * 374761393u + y * 668265263u + seed * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return h;
}

static float hashFloatCPU(unsigned int x, unsigned int y, unsigned int seed)
{
    return float(hashUIntCPU(x, y, seed)) / 4294967295.0f;
}

static float smoothstep5CPU(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static void generateGrainCPU(float* grain, int w, int h, float grainSize, float sharpness, unsigned int seed)
{
    float invSize = 1.0f / std::max(grainSize, 0.001f);
    float p = 1.0f / std::max(0.1f, 1.0f + sharpness * 3.0f);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float fx = float(x) * invSize;
            float fy = float(y) * invSize;

            int ix = int(floorf(fx));
            int iy = int(floorf(fy));
            float fracX = fx - float(ix);
            float fracY = fy - float(iy);

            float sx = smoothstep5CPU(fracX);
            float sy = smoothstep5CPU(fracY);

            float v00 = hashFloatCPU(ix, iy, seed);
            float v10 = hashFloatCPU(ix + 1, iy, seed);
            float v01 = hashFloatCPU(ix, iy + 1, seed);
            float v11 = hashFloatCPU(ix + 1, iy + 1, seed);

            float v0 = v00 + sx * (v10 - v00);
            float v1 = v01 + sx * (v11 - v01);
            float noise = v0 + sy * (v1 - v0);

            float centered = noise * 2.0f - 1.0f;
            float shaped = std::pow(std::fabs(centered), p);
            if (centered < 0.0f) shaped = -shaped;
            float result = shaped * 0.5f + 0.5f;

            grain[y * w + x] = clampf(result, 0.0f, 1.0f);
        }
    }
}

static void applyGrainCPU(
    const float* src, float* dst, const float* grain,
    int w, int h, int srcStride, int dstStride,
    float intensity, float mix, int colorMode, unsigned int seed)
{
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int si = y * srcStride + x * 4;
            int di = y * dstStride + x * 4;

            float r = src[si + 0];
            float g = src[si + 1];
            float b = src[si + 2];
            float a = src[si + 3];

            if (colorMode) {
                float gr = (hashFloatCPU(x, y, seed + 0) * 0.3f + grain[y * w + x] * 0.7f) * 2.0f - 1.0f;
                float gg = (hashFloatCPU(x, y, seed + 1) * 0.3f + grain[y * w + x] * 0.7f) * 2.0f - 1.0f;
                float gb = (hashFloatCPU(x, y, seed + 2) * 0.3f + grain[y * w + x] * 0.7f) * 2.0f - 1.0f;

                float nr = clampf(r * (1.0f + gr * intensity), 0.0f, 1.0f);
                float ng = clampf(g * (1.0f + gg * intensity), 0.0f, 1.0f);
                float nb = clampf(b * (1.0f + gb * intensity), 0.0f, 1.0f);

                dst[di + 0] = r + mix * (nr - r);
                dst[di + 1] = g + mix * (ng - g);
                dst[di + 2] = b + mix * (nb - b);
            } else {
                float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                float gv = (grain[y * w + x] * 2.0f - 1.0f) * intensity * (1.0f - luma * 0.5f);
                float nr = clampf(r * (1.0f + gv), 0.0f, 1.0f);
                float ng = clampf(g * (1.0f + gv), 0.0f, 1.0f);
                float nb = clampf(b * (1.0f + gv), 0.0f, 1.0f);

                dst[di + 0] = r + mix * (nr - r);
                dst[di + 1] = g + mix * (ng - g);
                dst[di + 2] = b + mix * (nb - b);
            }
            dst[di + 3] = a;
        }
    }
}

// ---- Unit Tests ----

static void test_clampf_below()
{
    TEST("clampf clamps value below range");
    CHECK(clampf(-0.5f, 0.0f, 1.0f) == 0.0f);
    PASS();
}

static void test_clampf_above()
{
    TEST("clampf clamps value above range");
    CHECK(clampf(1.5f, 0.0f, 1.0f) == 1.0f);
    PASS();
}

static void test_clampf_through()
{
    TEST("clampf passes through in-range value");
    CHECK(clampf(0.5f, 0.0f, 1.0f) == 0.5f);
    PASS();
}

static void test_hash_deterministic()
{
    TEST("hashUIntCPU is deterministic");
    CHECK(hashUIntCPU(42, 99, 1234) == hashUIntCPU(42, 99, 1234));
    PASS();
}

static void test_hash_differs_on_x()
{
    TEST("hashUIntCPU differs when x changes");
    CHECK(hashUIntCPU(1, 0, 0) != hashUIntCPU(2, 0, 0));
    PASS();
}

static void test_hash_differs_on_y()
{
    TEST("hashUIntCPU differs when y changes");
    CHECK(hashUIntCPU(0, 1, 0) != hashUIntCPU(0, 2, 0));
    PASS();
}

static void test_hash_differs_on_seed()
{
    TEST("hashUIntCPU differs when seed changes");
    CHECK(hashUIntCPU(0, 0, 1) != hashUIntCPU(0, 0, 2));
    PASS();
}

static void test_hash_float_in_range()
{
    TEST("hashFloatCPU returns value in [0,1]");
    for (unsigned int s = 0; s < 100; ++s) {
        float f = hashFloatCPU(s, s * 3, s * 7);
        CHECK(f >= 0.0f && f <= 1.0f);
    }
    PASS();
}

static void test_smoothstep5_zero()
{
    TEST("smoothstep5CPU(0) == 0");
    CHECK(smoothstep5CPU(0.0f) == 0.0f);
    PASS();
}

static void test_smoothstep5_one()
{
    TEST("smoothstep5CPU(1) == 1");
    CHECK(smoothstep5CPU(1.0f) == 1.0f);
    PASS();
}

static void test_smoothstep5_half()
{
    TEST("smoothstep5CPU(0.5) == 0.5");
    CHECK_NEAR(smoothstep5CPU(0.5f), 0.5f, 1e-6f);
    PASS();
}

static void test_smoothstep5_monotonic()
{
    TEST("smoothstep5CPU is monotonic");
    float prev = -1.0f;
    for (int i = 0; i <= 100; ++i) {
        float t = float(i) / 100.0f;
        float v = smoothstep5CPU(t);
        CHECK(v >= prev - 1e-6f);
        prev = v;
    }
    PASS();
}

static void test_smoothstep5_symmetric()
{
    TEST("smoothstep5CPU is symmetric about 0.5");
    for (int i = 0; i <= 50; ++i) {
        float t = float(i) / 100.0f;
        float v1 = smoothstep5CPU(t);
        float v2 = 1.0f - smoothstep5CPU(1.0f - t);
        CHECK_NEAR(v1, v2, 1e-6f);
    }
    PASS();
}

static void test_grain_generates_values_in_range()
{
    TEST("generateGrainCPU: all values in [0, 1]");
    const int w = 64, h = 64;
    float* grain = (float*)std::malloc(w * h * sizeof(float));
    generateGrainCPU(grain, w, h, 3.0f, 0.5f, 1234);
    for (int i = 0; i < w * h; ++i) {
        CHECK(grain[i] >= 0.0f && grain[i] <= 1.0f);
    }
    std::free(grain);
    PASS();
}

static void test_grain_deterministic_same_seed()
{
    TEST("generateGrainCPU: same seed => identical output");
    const int w = 32, h = 32;
    float* a = (float*)std::malloc(w * h * sizeof(float));
    float* b = (float*)std::malloc(w * h * sizeof(float));
    generateGrainCPU(a, w, h, 2.0f, 0.3f, 42);
    generateGrainCPU(b, w, h, 2.0f, 0.3f, 42);
    for (int i = 0; i < w * h; ++i) CHECK(a[i] == b[i]);
    std::free(a);
    std::free(b);
    PASS();
}

static void test_grain_different_seed()
{
    TEST("generateGrainCPU: different seed => different output");
    const int w = 16, h = 16;
    float* a = (float*)std::malloc(w * h * sizeof(float));
    float* b = (float*)std::malloc(w * h * sizeof(float));
    generateGrainCPU(a, w, h, 2.0f, 0.5f, 1);
    generateGrainCPU(b, w, h, 2.0f, 0.5f, 2);
    bool diffFound = false;
    for (int i = 0; i < w * h; ++i) {
        if (a[i] != b[i]) { diffFound = true; break; }
    }
    CHECK(diffFound);
    std::free(a);
    std::free(b);
    PASS();
}

static void test_grain_larger_size_smoother()
{
    TEST("generateGrainCPU: larger grain size => lower std dev (smoother)");
    const int w = 128, h = 128;
    float* small = (float*)std::malloc(w * h * sizeof(float));
    float* large = (float*)std::malloc(w * h * sizeof(float));
    generateGrainCPU(small, w, h, 1.0f, 0.5f, 99);
    generateGrainCPU(large, w, h, 8.0f, 0.5f, 99);

    float meanS = 0.0f, meanL = 0.0f;
    int n = w * h;
    for (int i = 0; i < n; ++i) { meanS += small[i]; meanL += large[i]; }
    meanS /= float(n); meanL /= float(n);

    float varS = 0.0f, varL = 0.0f;
    for (int i = 0; i < n; ++i) {
        varS += (small[i] - meanS) * (small[i] - meanS);
        varL += (large[i] - meanL) * (large[i] - meanL);
    }
    varS /= float(n); varL /= float(n);

    // Larger grain = lower spatial frequency = less variance in sampled pixels
    CHECK(varL < varS * 0.9f);
    std::free(small);
    std::free(large);
    PASS();
}

static void test_apply_luma_grain_identity_at_zero_intensity()
{
    TEST("applyGrainCPU: zero intensity => output equals input (luma mode)");
    const int w = 16, h = 16;
    const int tightStride = w * 4;
    float* src = (float*)std::malloc(static_cast<size_t>(w) * h * 4 * sizeof(float));
    float* dst = (float*)std::malloc(static_cast<size_t>(w) * h * 4 * sizeof(float));
    float* grain = (float*)std::malloc(static_cast<size_t>(w) * h * sizeof(float));

    for (int i = 0; i < w * h * 4; ++i) src[i] = float(i % 256) / 255.0f;
    generateGrainCPU(grain, w, h, 3.0f, 0.5f, 1234);

    applyGrainCPU(src, dst, grain, w, h, tightStride, tightStride, 0.0f, 1.0f, 0, 1234);
    for (int i = 0; i < w * h * 4; ++i) CHECK(dst[i] == src[i]);

    std::free(src); std::free(dst); std::free(grain);
    PASS();
}

static void test_apply_color_grain_identity_at_zero_intensity()
{
    TEST("applyGrainCPU: zero intensity => output equals input (color mode)");
    const int w = 16, h = 16;
    const int tightStride = w * 4;
    float* src = (float*)std::malloc(static_cast<size_t>(w) * h * 4 * sizeof(float));
    float* dst = (float*)std::malloc(static_cast<size_t>(w) * h * 4 * sizeof(float));
    float* grain = (float*)std::malloc(static_cast<size_t>(w) * h * sizeof(float));

    for (int i = 0; i < w * h * 4; ++i) src[i] = float(i % 256) / 255.0f;
    generateGrainCPU(grain, w, h, 3.0f, 0.5f, 1234);

    applyGrainCPU(src, dst, grain, w, h, tightStride, tightStride, 0.0f, 1.0f, 1, 1234);
    for (int i = 0; i < w * h * 4; ++i) CHECK(dst[i] == src[i]);

    std::free(src); std::free(dst); std::free(grain);
    PASS();
}

static void test_apply_grain_mix_zero()
{
    TEST("applyGrainCPU: mix=0 => output equals input");
    const int w = 8, h = 8;
    const int tightStride = w * 4;
    float* src = (float*)std::malloc(static_cast<size_t>(w) * h * 4 * sizeof(float));
    float* dst = (float*)std::malloc(static_cast<size_t>(w) * h * 4 * sizeof(float));
    float* grain = (float*)std::malloc(static_cast<size_t>(w) * h * sizeof(float));

    for (int i = 0; i < w * h * 4; ++i) src[i] = float(i) / float(w * h * 4);
    generateGrainCPU(grain, w, h, 2.0f, 0.5f, 7);

    applyGrainCPU(src, dst, grain, w, h, tightStride, tightStride, 1.0f, 0.0f, 0, 7);
    for (int i = 0; i < w * h * 4; ++i) CHECK(dst[i] == src[i]);

    std::free(src); std::free(dst); std::free(grain);
    PASS();
}

static void test_apply_grain_non_identity()
{
    TEST("applyGrainCPU: positive intensity changes output (luma mode)");
    const int w = 32, h = 32;
    const int tightStride = w * 4;
    float* src = (float*)std::calloc(static_cast<size_t>(w) * h * 4, sizeof(float));
    float* dst = (float*)std::calloc(static_cast<size_t>(w) * h * 4, sizeof(float));
    float* grain = (float*)std::malloc(static_cast<size_t>(w) * h * sizeof(float));

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int i = y * tightStride + x * 4;
            src[i + 0] = 0.5f; src[i + 1] = 0.5f;
            src[i + 2] = 0.5f; src[i + 3] = 1.0f;
        }

    generateGrainCPU(grain, w, h, 2.0f, 0.5f, 42);
    applyGrainCPU(src, dst, grain, w, h, tightStride, tightStride, 0.5f, 1.0f, 0, 42);

    bool changed = false;
    for (int y = 0; y < h && !changed; ++y)
        for (int x = 0; x < w && !changed; ++x) {
            int i = y * tightStride + x * 4;
            if (dst[i + 0] != src[i + 0] ||
                dst[i + 1] != src[i + 1] ||
                dst[i + 2] != src[i + 2]) changed = true;
        }
    CHECK(changed);

    std::free(src); std::free(dst); std::free(grain);
    PASS();
}

static void test_apply_color_grain_channels_differ()
{
    TEST("applyGrainCPU: color mode produces different channel values");
    const int w = 16, h = 16;
    const int tightStride = w * 4;
    float* src = (float*)std::calloc(static_cast<size_t>(w) * h * 4, sizeof(float));
    float* dst = (float*)std::calloc(static_cast<size_t>(w) * h * 4, sizeof(float));
    float* grain = (float*)std::malloc(static_cast<size_t>(w) * h * sizeof(float));

    for (int i = 0; i < w * h * 4; ++i) src[i] = 0.5f;
    generateGrainCPU(grain, w, h, 2.0f, 0.5f, 99);
    applyGrainCPU(src, dst, grain, w, h, tightStride, tightStride, 0.8f, 1.0f, 1, 99);

    bool channelsDiffer = false;
    for (int y = 0; y < h && !channelsDiffer; ++y)
        for (int x = 0; x < w && !channelsDiffer; ++x) {
            int i = y * tightStride + x * 4;
            if (dst[i + 0] != dst[i + 1] || dst[i + 1] != dst[i + 2])
                channelsDiffer = true;
        }
    CHECK(channelsDiffer);

    std::free(src); std::free(dst); std::free(grain);
    PASS();
}

static void test_alpha_preserved()
{
    TEST("applyGrainCPU: alpha channel is always preserved");
    const int w = 8, h = 8;
    const int tightStride = w * 4;
    float src[8 * 8 * 4];
    float dst[8 * 8 * 4];
    float grain[8 * 8];

    for (int i = 0; i < w * h * 4; ++i) src[i] = 0.5f;
    generateGrainCPU(grain, w, h, 3.0f, 0.5f, 1);
    applyGrainCPU(src, dst, grain, w, h, tightStride, tightStride, 1.0f, 1.0f, 1, 1);

    for (int i = 3; i < w * h * 4; i += 4) CHECK(dst[i] == src[i]);
    PASS();
}

static void test_luma_grain_channel_uniformity()
{
    TEST("applyGrainCPU: luma mode applies same factor to RGB");
    const int w = 16, h = 16;
    const int tightStride = w * 4;
    float* src = (float*)std::malloc(static_cast<size_t>(w) * h * 4 * sizeof(float));
    float* dst = (float*)std::malloc(static_cast<size_t>(w) * h * 4 * sizeof(float));
    float* grain = (float*)std::malloc(static_cast<size_t>(w) * h * sizeof(float));

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int i = y * tightStride + x * 4;
            src[i+0] = 0.1f; src[i+1] = 0.2f; src[i+2] = 0.3f; src[i+3] = 1.0f;
        }

    // Use moderate intensity to avoid clamping at 1.0
    generateGrainCPU(grain, w, h, 2.0f, 0.5f, 5);
    applyGrainCPU(src, dst, grain, w, h, tightStride, tightStride, 0.3f, 1.0f, 0, 5);

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int si = y * tightStride + x * 4;
            int di = si;
            // In luma mode, all channels get the same grain multiplier
            // Check that the result ratios match: dr/src_r == dg/src_g == db/src_b
            float dr = dst[di+0] - src[si+0];
            float dg = dst[di+1] - src[si+1];
            float db = dst[di+2] - src[si+2];
            // All differences should be zero or have the same sign and ratio
            if (std::fabs(dr) > 1e-6f && std::fabs(dg) > 1e-6f && std::fabs(db) > 1e-6f) {
                CHECK_NEAR(dr / (src[si+0] + 1e-6f), dg / (src[si+1] + 1e-6f), 1e-4f);
                CHECK_NEAR(dr / (src[si+0] + 1e-6f), db / (src[si+2] + 1e-6f), 1e-4f);
            }
        }

    std::free(src); std::free(dst); std::free(grain);
    PASS();
}

int main()
{
    std::printf("filmgrain-test: Film Grain Plugin Unit Tests\n");
    std::printf("=============================================\n\n");

    test_clampf_below();
    test_clampf_above();
    test_clampf_through();
    test_hash_deterministic();
    test_hash_differs_on_x();
    test_hash_differs_on_y();
    test_hash_differs_on_seed();
    test_hash_float_in_range();
    test_smoothstep5_zero();
    test_smoothstep5_one();
    test_smoothstep5_half();
    test_smoothstep5_monotonic();
    test_smoothstep5_symmetric();
    test_grain_generates_values_in_range();
    test_grain_deterministic_same_seed();
    test_grain_different_seed();
    test_grain_larger_size_smoother();
    test_apply_luma_grain_identity_at_zero_intensity();
    test_apply_color_grain_identity_at_zero_intensity();
    test_apply_grain_mix_zero();
    test_apply_grain_non_identity();
    test_apply_color_grain_channels_differ();
    test_alpha_preserved();
    test_luma_grain_channel_uniformity();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);
    return (s_passCount == s_testCount) ? 0 : 1;
}
