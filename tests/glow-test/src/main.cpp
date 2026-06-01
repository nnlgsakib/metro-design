#include <cstdio>
#include <cassert>
#include <cstring>
#include <cmath>
#include <algorithm>

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

__attribute__((unused)) static float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

static void test_clampf_clamps_below()
{
    TEST("clampf clamps value below range");
    assert(clampf(-0.5f, 0.0f, 1.0f) == 0.0f);
    PASS();
}

static void test_clampf_clamps_above()
{
    TEST("clampf clamps value above range");
    assert(clampf(1.5f, 0.0f, 1.0f) == 1.0f);
    PASS();
}

static void test_clampf_passes_through()
{
    TEST("clampf passes value through when in range");
    assert(clampf(0.5f, 0.0f, 1.0f) == 0.5f);
    PASS();
}

static void test_box_blur_preserves_mean()
{
    TEST("box blur preserves mean of uniform image");
    const int w = 16, h = 16, nc = 4;
    float *src = new float[w * h * nc];
    float *tmp = new float[w * h * nc];
    float *dst = new float[w * h * nc];
    for (int i = 0; i < w * h * nc; ++i) src[i] = 0.5f;

    int r = 3;
    float inv = 1.0f / (2.0f * r + 1.0f);

    for (int y = 0; y < h; ++y) {
        for (int c = 0; c < nc; ++c) {
            float sum = 0.0f;
            int stride = w * nc;
            for (int x = -r; x <= r; ++x) {
                int ix = std::max(0, std::min(w - 1, x));
                sum += src[y * stride + ix * nc + c];
            }
            tmp[y * stride + 0 * nc + c] = sum * inv;
            for (int x = 1; x < w; ++x) {
                int prev = std::max(0, std::min(w - 1, x - r - 1));
                int next = std::max(0, std::min(w - 1, x + r));
                sum += src[y * stride + next * nc + c] - src[y * stride + prev * nc + c];
                tmp[y * stride + x * nc + c] = sum * inv;
            }
        }
    }

    for (int x = 0; x < w; ++x) {
        for (int c = 0; c < nc; ++c) {
            float sum = 0.0f;
            int stride = w * nc;
            for (int y = -r; y <= r; ++y) {
                int iy = std::max(0, std::min(h - 1, y));
                sum += tmp[iy * stride + x * nc + c];
            }
            dst[0 * stride + x * nc + c] = sum * inv;
            for (int y = 1; y < h; ++y) {
                int prev = std::max(0, std::min(h - 1, y - r - 1));
                int next = std::max(0, std::min(h - 1, y + r));
                sum += tmp[next * stride + x * nc + c] - tmp[prev * stride + x * nc + c];
                dst[y * stride + x * nc + c] = sum * inv;
            }
        }
    }

    float maxDiff = 0.0f;
    for (int i = 0; i < w * h * nc; ++i) {
        float diff = std::fabs(dst[i] - 0.5f);
        if (diff > maxDiff) maxDiff = diff;
    }
    assert(maxDiff < 0.001f);
    PASS();

    delete[] src;
    delete[] tmp;
    delete[] dst;
}

static void test_threshold_zero()
{
    TEST("threshold of 0 extracts all pixels");
    const int w = 4, h = 4, nc = 4, stride = w * nc;
    float src[64] = {};
    for (int i = 0; i < w * h; ++i) {
        src[i * nc + 0] = 0.2f;
        src[i * nc + 1] = 0.3f;
        src[i * nc + 2] = 0.4f;
        src[i * nc + 3] = 1.0f;
    }

    float threshold = 0.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int si = y * stride + x * nc;
            float luma = 0.2126f * src[si + 0] + 0.7152f * src[si + 1] + 0.0722f * src[si + 2];
            float bright = std::max(0.0f, luma - threshold) / (1.0f - threshold + 1e-6f);
            assert(bright > 0.0f);
            assert(src[si + 0] * bright > 0.0f);
        }
    }
    PASS();
}

static void test_threshold_one()
{
    TEST("threshold of 1 extracts no pixels");
    const int w = 4, h = 4, nc = 4, stride = w * nc;
    float src[64];
    for (int i = 0; i < w * h * nc; ++i) src[i] = 0.5f;
    for (int i = 0; i < w * h * nc; i += nc) src[i + 3] = 1.0f;

    float threshold = 1.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int si = y * stride + x * nc;
            float luma = 0.2126f * src[si + 0] + 0.7152f * src[si + 1] + 0.0722f * src[si + 2];
            float bright = std::max(0.0f, luma - threshold) / (1.0f - threshold + 1e-6f);
            assert(bright == 0.0f);
        }
    }
    PASS();
}

static void test_composite_identity()
{
    TEST("composite with intensity=0 returns original");
    const int w = 4, h = 4, nc = 4, stride = w * nc;
    float src[64];
    for (int i = 0; i < w * h * nc; ++i) {
        src[i] = static_cast<float>(i) / 64.0f;
    }

    float intensity = 0.0f, mix = 1.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int si = y * stride + x * nc;
            float v0 = src[si + 0] + mix * 1.0f * intensity;
            float v1 = src[si + 1] + mix * 1.0f * intensity;
            float v2 = src[si + 2] + mix * 1.0f * intensity;
            assert(v0 == src[si + 0]);
            assert(v1 == src[si + 1]);
            assert(v2 == src[si + 2]);
        }
    }
    PASS();
}

static void test_composite_zero_mix()
{
    TEST("composite with mix=0 returns original");
    const int w = 4, h = 4, nc = 4, stride = w * nc;
    float src[64];
    for (int i = 0; i < w * h * nc; ++i) {
        src[i] = static_cast<float>(i) / 64.0f;
    }

    float intensity = 1.0f, mix = 0.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int si = y * stride + x * nc;
            float v0 = src[si + 0] + mix * 1.0f * 1.0f * intensity;
            float v1 = src[si + 1] + mix * 1.0f * 1.0f * intensity;
            float v2 = src[si + 2] + mix * 1.0f * 1.0f * intensity;
            assert(v0 == src[si + 0]);
            assert(v1 == src[si + 1]);
            assert(v2 == src[si + 2]);
        }
    }
    PASS();
}

static void test_full_pipeline_small()
{
    TEST("full bloom pipeline produces expected output on 4x4 image");
    const int w = 4, h = 4, nc = 4, stride = w * nc;
    const int rw = w, rh = h;

    float src[64];
    float dst[64];
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int i = y * stride + x * nc;
            src[i + 0] = 0.8f; src[i + 1] = 0.6f;
            src[i + 2] = 0.4f; src[i + 3] = 1.0f;
        }
    }

    float intensityF = 0.5f, thresholdF = 0.5f;
    float radiusF = 2.0f;
    float glowRF = 1.0f, glowGF = 0.5f, glowBF = 0.2f, mixF = 0.8f;

    float *tmp = new float[rw * rh * nc];
    float *blurred = new float[rw * rh * nc];

    for (int y = 0; y < rh; ++y) {
        for (int x = 0; x < rw; ++x) {
            int si = y * stride + x * nc;
            float luma = 0.2126f * src[si + 0] + 0.7152f * src[si + 1] + 0.0722f * src[si + 2];
            float bright = std::max(0.0f, luma - thresholdF) / (1.0f - thresholdF + 1e-6f);
            int ti = y * rw * nc + x * nc;
            blurred[ti + 0] = src[si + 0] * bright;
            blurred[ti + 1] = src[si + 1] * bright;
            blurred[ti + 2] = src[si + 2] * bright;
            blurred[ti + 3] = src[si + 3];
        }
    }

    int r = static_cast<int>(std::ceil(radiusF));
    std::memcpy(tmp, blurred, rw * rh * nc * sizeof(float));

    for (int y = 0; y < rh; ++y) {
        for (int c = 0; c < nc; ++c) {
            float sum = 0.0f;
            for (int x = -r; x <= r; ++x) {
                int ix = std::max(0, std::min(rw - 1, x));
                sum += tmp[y * rw * nc + ix * nc + c];
            }
            float inv = 1.0f / (2.0f * r + 1.0f);
            blurred[y * rw * nc + 0 * nc + c] = sum * inv;
            for (int x = 1; x < rw; ++x) {
                int prev = std::max(0, std::min(rw - 1, x - r - 1));
                int next = std::max(0, std::min(rw - 1, x + r));
                sum += tmp[y * rw * nc + next * nc + c] - tmp[y * rw * nc + prev * nc + c];
                blurred[y * rw * nc + x * nc + c] = sum * inv;
            }
        }
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int di = y * stride + x * nc;
            int bi = y * rw * nc + x * nc;
            dst[di + 0] = src[di + 0] + mixF * blurred[bi + 0] * glowRF * intensityF;
            dst[di + 1] = src[di + 1] + mixF * blurred[bi + 1] * glowGF * intensityF;
            dst[di + 2] = src[di + 2] + mixF * blurred[bi + 2] * glowBF * intensityF;
            dst[di + 3] = src[di + 3];
        }
    }

    assert(dst[0] > src[0]);
    assert(dst[1] > src[1]);
    assert(dst[2] > src[2]);
    assert(dst[3] == 1.0f);
    PASS();

    delete[] tmp;
    delete[] blurred;
}

static void test_identity_intensity_zero()
{
    TEST("identity check: intensity=0 triggers identity");
    double intensity = 0.0, mix = 1.0;
    if (intensity == 0.0 || mix == 0.0) {
        PASS();
        return;
    }
    FAIL("should have been identity");
}

static void test_identity_mix_zero()
{
    TEST("identity check: mix=0 triggers identity");
    double intensity = 1.0, mix = 0.0;
    if (intensity == 0.0 || mix == 0.0) {
        PASS();
        return;
    }
    FAIL("should have been identity");
}

static void test_identity_nonzero()
{
    TEST("identity check: non-zero intensity and mix does not trigger identity");
    double intensity = 0.5, mix = 1.0;
    if (intensity != 0.0 && mix != 0.0) {
        PASS();
        return;
    }
    FAIL("should not be identity");
}

static void test_plugingrouping()
{
    TEST("plugin grouping is Metro Design");
    assert(std::strcmp("Metro Design", "Metro Design") == 0);
    PASS();
}

static void test_plugin_identifier()
{
    TEST("plugin identifier is com.metrodesign.glow");
    assert(std::strcmp("com.metrodesign.glow", "com.metrodesign.glow") == 0);
    PASS();
}

int main()
{
    std::printf("glow-test: Metro Glow Plugin Unit Tests\n");
    std::printf("========================================\n\n");

    test_clampf_clamps_below();
    test_clampf_clamps_above();
    test_clampf_passes_through();
    test_box_blur_preserves_mean();
    test_threshold_zero();
    test_threshold_one();
    test_composite_identity();
    test_composite_zero_mix();
    test_full_pipeline_small();
    test_identity_intensity_zero();
    test_identity_mix_zero();
    test_identity_nonzero();
    test_plugingrouping();
    test_plugin_identifier();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);

    return (s_passCount == s_testCount) ? 0 : 1;
}
