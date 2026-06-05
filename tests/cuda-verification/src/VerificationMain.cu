#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cfloat>
#include <algorithm>

// --- Direct declarations matching kernel .cu definitions ---

extern "C" {
bool launchGenerateGrain(float* grain, int w, int h, float grainSize, float sharpness, unsigned int seed);
bool launchApplyGrain(const float* src, float* dst, const float* grain, int w, int h, int srcStride, int dstStride, float intensity, float mix, int colorMode, unsigned int seed);
}

struct FlareParams {
    float brightness;
    float flareSize;
    int ghostCount;
    float anamorphicStretch;
    float chromaShift;
    float hueTint;
    float mix;
    float centerX;
    float centerY;
    float imgCx;
    float imgCy;
    float axisDx;
    float axisDy;
    float axisLen;
};

bool launchGlowGPU(const float *src, float *dst,
                   int srcStride, int dstStride,
                   int rx1, int ry1, int rx2, int ry2,
                   float intensity, float threshold, float radius,
                   float glowR, float glowG, float glowB, float mix);

bool launchLensFlareGPU(const float *src, float *dst,
                        int srcStride, int dstStride,
                        int rx1, int ry1, int rx2, int ry2,
                        int srcW, int srcH,
                        const FlareParams &params);

static int s_testCount = 0;
static int s_passCount = 0;
static int s_failCount = 0;

#define TEST(name) do { \
    s_testCount++; \
    std::printf("  TEST: %s ... ", name); \
} while(0)

#define PASS() do { s_passCount++; std::printf("PASS\n"); } while(0)
#define FAIL(msg) do { \
    s_failCount++; \
    std::printf("FAIL at %s:%d\n  %s\n", __FILE__, __LINE__, msg); \
} while(0)

#define REQUIRE(cond, msg) do { \
    if (!(cond)) { \
        s_failCount++; \
        std::printf("FAIL at %s:%d\n  %s: '%s' is false\n", __FILE__, __LINE__, msg, #cond); \
        return; \
    } \
} while(0)

#define CHECK_NEAR(a, b, eps, msg) do { \
    float _a = (a), _b = (b); \
    if (std::fabs(_a - _b) > (eps)) { \
        s_failCount++; \
        std::printf("FAIL at %s:%d\n  %s: %f != %f (eps=%f)\n", __FILE__, __LINE__, msg, _a, _b, (float)(eps)); \
        return; \
    } \
} while(0)

// ============================================================
// CPU Reference Implementations
// ============================================================

// --- FilmGrain CPU Reference ---

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
    for (int y = 0; y < h; ++y)
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
            grain[y * w + x] = clampf(shaped * 0.5f + 0.5f, 0.0f, 1.0f);
        }
}

static void applyGrainCPU(
    const float* src, float* dst, const float* grain,
    int w, int h, int srcStride, int dstStride,
    float intensity, float mix, int colorMode, unsigned int seed)
{
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int si = y * srcStride + x * 4;
            int di = y * dstStride + x * 4;
            float r = src[si + 0], g = src[si + 1], b = src[si + 2], a = src[si + 3];
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

// --- Glow CPU Reference ---

static void glowThresholdCPU(const float *src, float *dst,
                              int srcRowStride, int dstRowStride,
                              int w, int h, int nc, float threshold)
{
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int si = y * srcRowStride + x * nc;
            int di = y * dstRowStride + x * nc;
            float luma = 0.2126f * src[si + 0] + 0.7152f * src[si + 1] + 0.0722f * src[si + 2];
            float bright = std::max(0.0f, luma - threshold) / (1.0f - threshold + 1e-6f);
            dst[di + 0] = src[si + 0] * bright;
            dst[di + 1] = src[si + 1] * bright;
            dst[di + 2] = src[si + 2] * bright;
            dst[di + 3] = src[si + 3];
        }
}

static void blurHCPU(const float *src, float *dst,
                     int rowStride, int w, int h, int nc, int radius)
{
    float inv = 1.0f / (2.0f * radius + 1.0f);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < nc; ++c) {
                float sum = 0.0f;
                for (int dx = -radius; dx <= radius; ++dx) {
                    int ix = std::min(std::max(0, x + dx), w - 1);
                    sum += src[y * rowStride + ix * nc + c];
                }
                dst[y * rowStride + x * nc + c] = sum * inv;
            }
}

static void blurVCPU(const float *src, float *dst,
                     int rowStride, int w, int h, int nc, int radius)
{
    float inv = 1.0f / (2.0f * radius + 1.0f);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < nc; ++c) {
                float sum = 0.0f;
                for (int dy = -radius; dy <= radius; ++dy) {
                    int iy = std::min(std::max(0, y + dy), h - 1);
                    sum += src[iy * rowStride + x * nc + c];
                }
                dst[y * rowStride + x * nc + c] = sum * inv;
            }
}

static void compositeCPU(const float *orig, const float *glow, float *dst,
                          int origStride, int glowStride, int dstStride,
                          int w, int h, int nc,
                          float intensity, float mix,
                          float glowR, float glowG, float glowB)
{
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int oi = y * origStride + x * nc;
            int gi = y * glowStride + x * nc;
            int di = y * dstStride + x * nc;
            dst[di + 0] = orig[oi + 0] + mix * glow[gi + 0] * glowR * intensity;
            dst[di + 1] = orig[oi + 1] + mix * glow[gi + 1] * glowG * intensity;
            dst[di + 2] = orig[oi + 2] + mix * glow[gi + 2] * glowB * intensity;
            dst[di + 3] = orig[oi + 3];
        }
}

// --- LensFlare CPU Reference ---

static float gaussianCPU(float x, float sigma)
{
    return std::exp(-(x * x) / (2.0f * sigma * sigma));
}

static void hsvToRgbCPU(float h, float s, float v, float &r, float &g, float &b)
{
    float c = v * s;
    float hp = h / 60.0f;
    float xv = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    float m = v - c;
    int hi = ((int)hp) % 6;
    switch (hi) {
        case 0: r = c; g = xv; b = 0.0f; break;
        case 1: r = xv; g = c; b = 0.0f; break;
        case 2: r = 0.0f; g = c; b = xv; break;
        case 3: r = 0.0f; g = xv; b = c; break;
        case 4: r = xv; g = 0.0f; b = c; break;
        case 5: r = c; g = 0.0f; b = xv; break;
        default: r = 0.0f; g = 0.0f; b = 0.0f; break;
    }
    r += m; g += m; b += m;
}

static void lensFlareCPU(const float *src, float *dst,
                          int srcStride, int dstStride,
                          int w, int h, int nc,
                          float brightness, float flareSize,
                          int ghostCount, float anamorphicStretch,
                          float chromaShift,
                          float tintR, float tintG, float tintB,
                          float mix,
                          float cfx, float cfy,
                          float imgCx, float imgCy)
{
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int si = y * srcStride + x * nc;
            int di = y * dstStride + x * nc;
            float origR = src[si + 0], origG = src[si + 1], origB = src[si + 2], origA = src[si + 3];
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);
            float axisDx = imgCx - cfx;
            float axisDy = imgCy - cfy;
            float axisLen = std::sqrt(axisDx * axisDx + axisDy * axisDy);
            if (axisLen < 0.001f) { axisDx = 1.0f; axisDy = 0.0f; }
            else { axisDx /= axisLen; axisDy /= axisLen; }
            float dx = fx - cfx;
            float dy = fy - cfy;
            float dist = std::sqrt(dx * dx + dy * dy);
            float flareR = 0.0f, flareG = 0.0f, flareB = 0.0f;
            if (brightness > 0.0f) {
                float centralSigma = std::max(8.0f * flareSize, 1.0f);
                float centralIntensity = brightness * gaussianCPU(dist, centralSigma);
                flareR += centralIntensity;
                flareG += centralIntensity;
                flareB += centralIntensity;
                if (anamorphicStretch > 0.0f) {
                    float perpDist = std::fabs(dx * (-axisDy) + dy * axisDx);
                    float streakSigma = std::max(4.0f * flareSize, 1.0f);
                    float streakIntensity = brightness * anamorphicStretch * gaussianCPU(perpDist, streakSigma);
                    flareR += streakIntensity;
                    flareG += streakIntensity;
                    flareB += streakIntensity;
                }
                float ghostSpacing = std::max(30.0f * flareSize, 5.0f);
                for (int gi = 0; gi < ghostCount; ++gi) {
                    float t = static_cast<float>(gi + 1) * 0.18f;
                    float gx = cfx + axisDx * ghostSpacing * t;
                    float gy = cfy + axisDy * ghostSpacing * t;
                    float gdist = std::sqrt((fx - gx) * (fx - gx) + (fy - gy) * (fy - gy));
                    float ghostSigma = std::max(10.0f * flareSize * (1.0f + t * 0.5f), 1.0f);
                    float ghostIntensity = brightness * gaussianCPU(gdist, ghostSigma)
                                         * (0.7f - static_cast<float>(gi) * 0.06f);
                    if (ghostIntensity > 0.0f) {
                        float gr = ghostIntensity, gg = ghostIntensity, gb = ghostIntensity;
                        if (chromaShift > 0.0f) {
                            float clum = 0.2126f * gr + 0.7152f * gg + 0.0722f * gb;
                            gr = ghostIntensity * (clum + (gr - clum) * chromaShift);
                            gg = ghostIntensity * (clum + (gg - clum) * chromaShift);
                            gb = ghostIntensity * (clum + (gb - clum) * chromaShift);
                        }
                        float tintBlend = 0.3f;
                        flareR += gr * (1.0f - tintBlend) + gr * tintBlend * tintR;
                        flareG += gg * (1.0f - tintBlend) + gg * tintBlend * tintG;
                        flareB += gb * (1.0f - tintBlend) + gb * tintBlend * tintB;
                    }
                }
            }
            dst[di + 0] = origR + mix * flareR;
            dst[di + 1] = origG + mix * flareG;
            dst[di + 2] = origB + mix * flareB;
            dst[di + 3] = origA;
        }
    }
}

// ============================================================
// Comparison Helpers
// ============================================================

static bool compareFloatBuffers(const float *gpu, const float *cpu,
                                 int count, float epsilon,
                                 const char *label, int *firstDiffIdx)
{
    for (int i = 0; i < count; ++i) {
        float diff = std::fabs(gpu[i] - cpu[i]);
        if (diff > epsilon) {
            if (firstDiffIdx) *firstDiffIdx = i;
            std::printf("  %s mismatch at idx %d: GPU=%f CPU=%f diff=%f\n",
                       label, i, gpu[i], cpu[i], diff);
            return false;
        }
    }
    return true;
}

// ============================================================
// CUDA Occupancy Measurement (uses launch wrappers)
// ============================================================

static void printDeviceInfo()
{
    cudaDeviceProp props;
    cudaGetDeviceProperties(&props, 0);
    printf("  Device: %s\n", props.name);
    printf("  Compute Capability: %d.%d\n", props.major, props.minor);
    printf("  Multiprocessors: %d\n", props.multiProcessorCount);
    printf("  CUDA Cores (est.): %d\n",
           props.multiProcessorCount *
           (props.major >= 2 ? (props.major >= 3 ? 192 : 48) : 8));
    printf("  Global Memory: %zu MB\n",
           props.totalGlobalMem / (1024 * 1024));
    printf("  L2 Cache: %d KB\n", props.l2CacheSize / 1024);
    printf("  Max Threads/Block: %d\n", props.maxThreadsPerBlock);
    printf("  Max Block Dimensions: %dx%dx%d\n",
           props.maxThreadsDim[0], props.maxThreadsDim[1], props.maxThreadsDim[2]);
    printf("  Max Grid Dimensions: %dx%dx%d\n",
           props.maxGridSize[0], props.maxGridSize[1], props.maxGridSize[2]);
    printf("  Memory Clock: %.0f MHz\n", props.memoryClockRate / 1000.0f);
    printf("  Memory Bus Width: %d bits\n", props.memoryBusWidth);
}

// ============================================================
// FilmGrain Verification Tests
// ============================================================

static void test_filmgrain_generate_grain_vs_cpu()
{
    TEST("Filmgrain: generateGrain GPU vs CPU (64x64, grainSize=3, sharpness=0.5)");
    const int w = 64, h = 64;
    float *h_grainGPU = new float[w * h];
    float *h_grainCPU = new float[w * h];

    REQUIRE(launchGenerateGrain(h_grainGPU, w, h, 3.0f, 0.5f, 1234),
            "launchGenerateGrain failed");
    generateGrainCPU(h_grainCPU, w, h, 3.0f, 0.5f, 1234);

    int firstDiff = -1;
    bool match = compareFloatBuffers(h_grainGPU, h_grainCPU, w * h, 1e-5f,
                                      "generateGrain", &firstDiff);
    REQUIRE(match, "GPU grain output does not match CPU reference");

    delete[] h_grainGPU;
    delete[] h_grainCPU;
    PASS();
}

static void test_filmgrain_generate_grain_zero_sharpness()
{
    TEST("Filmgrain: generateGrain GPU vs CPU (sharpness=0)");
    const int w = 32, h = 32;
    float *h_grainGPU = new float[w * h];
    float *h_grainCPU = new float[w * h];

    REQUIRE(launchGenerateGrain(h_grainGPU, w, h, 4.0f, 0.0f, 42),
            "launchGenerateGrain failed");
    generateGrainCPU(h_grainCPU, w, h, 4.0f, 0.0f, 42);

    bool match = compareFloatBuffers(h_grainGPU, h_grainCPU, w * h, 1e-5f,
                                      "generateGrain(sharp=0)", nullptr);
    REQUIRE(match, "GPU grain output does not match CPU reference at zero sharpness");

    delete[] h_grainGPU;
    delete[] h_grainCPU;
    PASS();
}

static void test_filmgrain_generate_grain_max_sharpness()
{
    TEST("Filmgrain: generateGrain GPU vs CPU (sharpness=10.0)");
    const int w = 16, h = 16;
    float *h_grainGPU = new float[w * h];
    float *h_grainCPU = new float[w * h];

    REQUIRE(launchGenerateGrain(h_grainGPU, w, h, 2.0f, 10.0f, 99),
            "launchGenerateGrain failed");
    generateGrainCPU(h_grainCPU, w, h, 2.0f, 10.0f, 99);

    bool match = compareFloatBuffers(h_grainGPU, h_grainCPU, w * h, 1e-5f,
                                      "generateGrain(sharp=10)", nullptr);
    REQUIRE(match, "GPU grain output does not match at max sharpness");

    delete[] h_grainGPU;
    delete[] h_grainCPU;
    PASS();
}

static void test_filmgrain_generate_min_grain_size()
{
    TEST("Filmgrain: generateGrain GPU vs CPU (min grain size)");
    const int w = 16, h = 16;
    float *h_grainGPU = new float[w * h];
    float *h_grainCPU = new float[w * h];

    REQUIRE(launchGenerateGrain(h_grainGPU, w, h, 0.0f, 0.5f, 7),
            "launchGenerateGrain failed");
    generateGrainCPU(h_grainCPU, w, h, 0.0f, 0.5f, 7);

    bool match = compareFloatBuffers(h_grainGPU, h_grainCPU, w * h, 1e-5f,
                                      "generateGrain(minSize)", nullptr);
    REQUIRE(match, "GPU grain output does not match at min grain size");

    delete[] h_grainGPU;
    delete[] h_grainCPU;
    PASS();
}

static void test_filmgrain_apply_luma_vs_cpu()
{
    TEST("Filmgrain: applyGrain GPU vs CPU (luma mode, intensity=0.5, mix=1.0)");
    const int w = 64, h = 64;
    const int stride = w * 4;
    float *h_src = new float[stride * h];
    float *h_dstGPU = new float[stride * h];
    float *h_dstCPU = new float[stride * h];
    float *h_grain = new float[w * h];

    for (int i = 0; i < stride * h; ++i) h_src[i] = float(i % 256) / 255.0f;
    for (int i = 3; i < stride * h; i += 4) h_src[i] = 1.0f;

    REQUIRE(launchGenerateGrain(h_grain, w, h, 3.0f, 0.5f, 1234),
            "launchGenerateGrain failed");
    REQUIRE(launchApplyGrain(h_src, h_dstGPU, h_grain, w, h, stride, stride,
                             0.5f, 1.0f, 0, 1234),
            "launchApplyGrain failed");
    applyGrainCPU(h_src, h_dstCPU, h_grain, w, h, stride, stride,
                  0.5f, 1.0f, 0, 1234);

    bool match = compareFloatBuffers(h_dstGPU, h_dstCPU, stride * h, 1e-5f,
                                      "applyGrain(luma)", nullptr);
    REQUIRE(match, "GPU applyGrain does not match CPU reference (luma mode)");

    delete[] h_src; delete[] h_dstGPU; delete[] h_dstCPU; delete[] h_grain;
    PASS();
}

static void test_filmgrain_apply_color_vs_cpu()
{
    TEST("Filmgrain: applyGrain GPU vs CPU (color mode)");
    const int w = 32, h = 32;
    const int stride = w * 4;
    float *h_src = new float[stride * h];
    float *h_dstGPU = new float[stride * h];
    float *h_dstCPU = new float[stride * h];
    float *h_grain = new float[w * h];

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int i = y * stride + x * 4;
            h_src[i+0] = 0.8f; h_src[i+1] = 0.5f; h_src[i+2] = 0.2f; h_src[i+3] = 1.0f;
        }

    REQUIRE(launchGenerateGrain(h_grain, w, h, 2.0f, 0.5f, 77),
            "launchGenerateGrain failed");
    REQUIRE(launchApplyGrain(h_src, h_dstGPU, h_grain, w, h, stride, stride,
                             0.7f, 1.0f, 1, 77),
            "launchApplyGrain failed");
    applyGrainCPU(h_src, h_dstCPU, h_grain, w, h, stride, stride,
                  0.7f, 1.0f, 1, 77);

    bool match = compareFloatBuffers(h_dstGPU, h_dstCPU, stride * h, 1e-5f,
                                      "applyGrain(color)", nullptr);
    REQUIRE(match, "GPU applyGrain does not match CPU reference (color mode)");

    delete[] h_src; delete[] h_dstGPU; delete[] h_dstCPU; delete[] h_grain;
    PASS();
}

static void test_filmgrain_generate_invalid_params()
{
    TEST("Filmgrain: generateGrain returns false for invalid params");
    float dummy[16];
    REQUIRE(launchGenerateGrain(nullptr, 4, 4, 1.0f, 0.5f, 1) == false,
            "null grain should fail");
    REQUIRE(launchGenerateGrain(dummy, 0, 4, 1.0f, 0.5f, 1) == false,
            "zero width should fail");
    REQUIRE(launchGenerateGrain(dummy, 4, 0, 1.0f, 0.5f, 1) == false,
            "zero height should fail");
    REQUIRE(launchGenerateGrain(dummy, -1, 4, 1.0f, 0.5f, 1) == false,
            "negative width should fail");
    PASS();
}

static void test_filmgrain_apply_invalid_params()
{
    TEST("Filmgrain: applyGrain returns false for invalid params");
    float dummy[64];
    float grain[64];
    REQUIRE(launchApplyGrain(nullptr, dummy, grain, 4, 4, 16, 16, 1.0f, 1.0f, 0, 1) == false,
            "null src should fail");
    REQUIRE(launchApplyGrain(dummy, nullptr, grain, 4, 4, 16, 16, 1.0f, 1.0f, 0, 1) == false,
            "null dst should fail");
    REQUIRE(launchApplyGrain(dummy, dummy, nullptr, 4, 4, 16, 16, 1.0f, 1.0f, 0, 1) == false,
            "null grain should fail");
    REQUIRE(launchApplyGrain(dummy, dummy, grain, 0, 4, 16, 16, 1.0f, 1.0f, 0, 1) == false,
            "zero width should fail");
    PASS();
}

// ============================================================
// Glow Verification Tests
// ============================================================

static void test_glow_pipeline_vs_cpu()
{
    TEST("Glow: full pipeline GPU vs CPU (32x32, threshold=0.5, radius=2)");
    const int w = 32, h = 32, nc = 4;
    const int stride = w * nc;
    float *h_src = new float[stride * h];
    float *h_dstGPU = new float[stride * h];
    float *h_dstCPU = new float[stride * h];

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int i = y * stride + x * nc;
            h_src[i+0] = 0.3f; h_src[i+1] = 0.6f;
            h_src[i+2] = 0.9f; h_src[i+3] = 1.0f;
        }

    REQUIRE(launchGlowGPU(h_src, h_dstGPU, stride, stride,
                           0, 0, w, h, 0.5f, 0.5f, 2.0f,
                           1.0f, 0.5f, 0.2f, 0.8f),
            "launchGlowGPU failed");

    int r = static_cast<int>(2.0f + 0.5f);
    if (r < 1) r = 1;
    int brightStride = w * nc;
    float *h_bright = new float[brightStride * h];
    float *h_tmp = new float[brightStride * h];
    float *h_blur = new float[brightStride * h];
    glowThresholdCPU(h_src, h_bright, stride, brightStride, w, h, nc, 0.5f);
    blurHCPU(h_bright, h_tmp, brightStride, w, h, nc, r);
    blurVCPU(h_tmp, h_blur, brightStride, w, h, nc, r);
    compositeCPU(h_src, h_blur, h_dstCPU, stride, brightStride, stride,
                 w, h, nc, 0.5f, 0.8f, 1.0f, 0.5f, 0.2f);

    bool match = compareFloatBuffers(h_dstGPU, h_dstCPU, stride * h, 1e-5f,
                                      "glowPipeline", nullptr);
    REQUIRE(match, "GPU glow pipeline does not match CPU reference");
    delete[] h_src; delete[] h_dstGPU; delete[] h_dstCPU;
    delete[] h_bright; delete[] h_tmp; delete[] h_blur;
    PASS();
}

static void test_glow_threshold_edge_cases()
{
    TEST("Glow: threshold edges (threshold=0, threshold=1)");
    // CPU-only verification; GPU comparison in pipeline test above
    const int w = 4, h = 4, nc = 4, stride = w * nc;
    float src[64];
    float dst0[64], dst1[64];
    for (int i = 0; i < w * h * nc; ++i) src[i] = 0.5f;
    for (int i = 3; i < w * h * nc; i += nc) src[i] = 1.0f;

    glowThresholdCPU(src, dst0, stride, stride, w, h, nc, 0.0f);
    glowThresholdCPU(src, dst1, stride, stride, w, h, nc, 1.0f);

    for (int i = 0; i < nc; ++i) {
        REQUIRE(dst0[i] > 0.0f, "threshold=0 should pass some pixels");
        REQUIRE(dst1[i] == 0.0f, "threshold=1 should suppress all pixels");
    }
    PASS();
}

static void test_glow_zero_radius()
{
    TEST("Glow: GPU pipeline with radius near zero");
    const int w = 8, h = 8, nc = 4;
    const int stride = w * nc;
    float *h_src = new float[stride * h];
    float *h_dstGPU = new float[stride * h];
    for (int i = 0; i < stride * h; ++i) h_src[i] = 0.5f;

    REQUIRE(launchGlowGPU(h_src, h_dstGPU, stride, stride,
                           0, 0, w, h, 0.3f, 0.5f, 0.1f,
                           1.0f, 1.0f, 1.0f, 1.0f),
            "launchGlowGPU with near-zero radius failed");
    // Verify output is reasonable (no NaN, no crash)
    for (int i = 0; i < stride * h; ++i)
        REQUIRE(std::isfinite(h_dstGPU[i]), "Output contains NaN");
    delete[] h_src; delete[] h_dstGPU;
    PASS();
}

// ============================================================
// LensFlare Verification Tests
// ============================================================

static void test_lensflare_pipeline_vs_cpu()
{
    TEST("LensFlare: full pipeline GPU vs CPU (64x64, standard params)");
    const int w = 64, h = 64, nc = 4;
    const int stride = w * nc;

    float *h_src = new float[stride * h];
    float *h_dstGPU = new float[stride * h];
    float *h_dstCPU = new float[stride * h];

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int i = y * stride + x * nc;
            h_src[i+0] = 0.2f; h_src[i+1] = 0.3f;
            h_src[i+2] = 0.4f; h_src[i+3] = 1.0f;
        }

    FlareParams params;
    params.brightness = 0.8f;
    params.flareSize = 1.0f;
    params.ghostCount = 3;
    params.anamorphicStretch = 0.3f;
    params.chromaShift = 0.2f;
    params.hueTint = 30.0f;
    params.mix = 0.7f;

    REQUIRE(launchLensFlareGPU(h_src, h_dstGPU, stride, stride,
                                0, 0, w, h, w, h, params),
            "launchLensFlareGPU failed");

    float tintR, tintG, tintB;
    hsvToRgbCPU(30.0f, 1.0f, 1.0f, tintR, tintG, tintB);
    float cfx = static_cast<float>(w) * 0.5f;
    float cfy = static_cast<float>(h) * 0.5f;
    float imgCx = cfx, imgCy = cfy;

    lensFlareCPU(h_src, h_dstCPU, stride, stride,
                 w, h, nc,
                 params.brightness, params.flareSize, params.ghostCount,
                 params.anamorphicStretch, params.chromaShift,
                 tintR, tintG, tintB, params.mix,
                 cfx, cfy, imgCx, imgCy);

    bool match = compareFloatBuffers(h_dstGPU, h_dstCPU, stride * h, 1e-4f,
                                      "lensFlarePipeline", nullptr);
    REQUIRE(match, "GPU lens flare does not match CPU reference");

    delete[] h_src; delete[] h_dstGPU; delete[] h_dstCPU;
    PASS();
}

static void test_lensflare_brightness_zero()
{
    TEST("LensFlare: brightness=0 is identity");
    const int w = 16, h = 16, nc = 4;
    const int stride = w * nc;
    float *h_src = new float[stride * h];
    float *h_dstGPU = new float[stride * h];

    for (int i = 0; i < stride * h; ++i) h_src[i] = float(i) / float(stride * h);

    FlareParams params = {};
    params.brightness = 0.0f;
    params.mix = 1.0f;

    REQUIRE(launchLensFlareGPU(h_src, h_dstGPU, stride, stride,
                                0, 0, w, h, w, h, params),
            "launchLensFlareGPU with brightness=0 failed");

    bool match = compareFloatBuffers(h_dstGPU, h_src, stride * h, 1e-6f,
                                      "lensFlare(zero brightness)", nullptr);
    REQUIRE(match, "GPU output should match input when brightness=0");

    delete[] h_src; delete[] h_dstGPU;
    PASS();
}

static void test_lensflare_edge_params()
{
    TEST("LensFlare: extreme params (max ghosts, large flare)");
    const int w = 32, h = 32, nc = 4;
    const int stride = w * nc;
    float *h_src = new float[stride * h];
    float *h_dstGPU = new float[stride * h];

    for (int i = 0; i < stride * h; ++i) h_src[i] = 0.5f;

    FlareParams params;
    params.brightness = 2.0f;
    params.flareSize = 5.0f;
    params.ghostCount = 10;
    params.anamorphicStretch = 1.0f;
    params.chromaShift = 1.0f;
    params.hueTint = 180.0f;
    params.mix = 1.0f;

    REQUIRE(launchLensFlareGPU(h_src, h_dstGPU, stride, stride,
                                0, 0, w, h, w, h, params),
            "launchLensFlareGPU with extreme params failed");
    for (int i = 0; i < stride * h; ++i)
        REQUIRE(std::isfinite(h_dstGPU[i]), "Output contains NaN with extreme params");

    delete[] h_src; delete[] h_dstGPU;
    PASS();
}

// ============================================================
// Edge Case & Cross-cutting Tests
// ============================================================

static void test_empty_rect()
{
    TEST("Edge: glow GPU with zero-area region returns false");
    float dummy[64];
    REQUIRE(launchGlowGPU(dummy, dummy, 16, 16, 0, 0, 0, 0,
                           0.5f, 0.5f, 2.0f, 1.0f, 1.0f, 1.0f, 0.8f) == false,
            "zero area should fail");
    REQUIRE(launchGlowGPU(dummy, dummy, 16, 16, 5, 5, 5, 10,
                           0.5f, 0.5f, 2.0f, 1.0f, 1.0f, 1.0f, 0.8f) == false,
            "zero width should fail");
    PASS();
}

static void test_lensflare_empty_rect()
{
    TEST("Edge: lensflare GPU with zero-area region returns false");
    float dummy[64];
    FlareParams params = {};
    params.brightness = 0.5f;
    REQUIRE(launchLensFlareGPU(dummy, dummy, 16, 16,
                                0, 0, 0, 0, 8, 8, params) == false,
            "zero area should fail");
    PASS();
}

static void test_all_black_image()
{
    TEST("Edge: all-black image (filmgrain apply luma mode)");
    const int w = 16, h = 16, stride = w * 4;
    float *h_src = new float[stride * h]();
    float *h_dst = new float[stride * h];
    float *h_grain = new float[w * h];

    REQUIRE(launchGenerateGrain(h_grain, w, h, 2.0f, 0.5f, 1),
            "generateGrain failed");
    REQUIRE(launchApplyGrain(h_src, h_dst, h_grain, w, h, stride, stride,
                             1.0f, 1.0f, 0, 1),
            "applyGrain on black image failed");
    // All-zero input should remain all-zero with luma-dependent grain
    // (luma is 0, so grain multiplier is (2*grain-1)*intensity*(1-0)=0)
    bool allZero = true;
    for (int y = 0; y < h && allZero; ++y)
        for (int x = 0; x < w && allZero; ++x) {
            int di = y * stride + x * 4;
            if (h_dst[di+0] != 0.0f || h_dst[di+1] != 0.0f || h_dst[di+2] != 0.0f)
                allZero = false;
        }
    REQUIRE(allZero, "Black image should stay black in luma mode");

    delete[] h_src; delete[] h_dst; delete[] h_grain;
    PASS();
}

static void test_all_white_image()
{
    TEST("Edge: all-white image (all plugins)");
    const int w = 8, h = 8, nc = 4, stride = w * nc;
    float *h_src = new float[stride * h];
    float *h_dst = new float[stride * h];
    float *h_grain = new float[w * h];
    for (int i = 0; i < stride * h; ++i) h_src[i] = 1.0f;

    REQUIRE(launchGenerateGrain(h_grain, w, h, 3.0f, 0.5f, 5),
            "generateGrain failed");
    REQUIRE(launchApplyGrain(h_src, h_dst, h_grain, w, h, stride, stride,
                             0.5f, 1.0f, 0, 5),
            "applyGrain on white image failed");
    for (int i = 0; i < stride * h; ++i)
        REQUIRE(std::isfinite(h_dst[i]), "applyGrain on white produced NaN");

    delete[] h_src; delete[] h_dst; delete[] h_grain;
    PASS();
}

// ============================================================
// Occupancy & Bandwidth Measurement
// ============================================================

static void test_device_info()
{
    TEST("Device Info");
    printDeviceInfo();
    PASS();
}

static void measure_bandwidth_1920x1080()
{
    TEST("Bandwidth: memcpy throughput estimate (1920x1080 float4)");
    const int w = 1920, h = 1080, nc = 4;
    size_t bufSize = static_cast<size_t>(w) * h * nc * sizeof(float);

    float *h_buf = new float[w * h * nc];
    float *d_buf = nullptr;

    cudaError_t err = cudaMalloc(&d_buf, bufSize);
    REQUIRE(err == cudaSuccess, "cudaMalloc failed for bandwidth test");

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);

    // Warmup
    cudaMemcpy(d_buf, h_buf, bufSize, cudaMemcpyHostToDevice);
    cudaDeviceSynchronize();

    const int iters = 10;
    cudaEventRecord(start);
    for (int i = 0; i < iters; ++i)
        cudaMemcpy(d_buf, h_buf, bufSize, cudaMemcpyHostToDevice);
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);

    float msH2D = 0.0f;
    cudaEventElapsedTime(&msH2D, start, stop);
    msH2D /= static_cast<float>(iters);
    double bwH2D = (static_cast<double>(bufSize) / 1.048576e6) / msH2D; // GB/s
    printf("  H2D bandwidth: %.2f GB/s (%.2f ms for %zu MB)\n",
           bwH2D, msH2D, bufSize / (1024 * 1024));

    cudaEventRecord(start);
    for (int i = 0; i < iters; ++i)
        cudaMemcpy(h_buf, d_buf, bufSize, cudaMemcpyDeviceToHost);
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);

    float msD2H = 0.0f;
    cudaEventElapsedTime(&msD2H, start, stop);
    msD2H /= static_cast<float>(iters);
    double bwD2H = (static_cast<double>(bufSize) / 1.048576e6) / msD2H;
    printf("  D2H bandwidth: %.2f GB/s (%.2f ms for %zu MB)\n",
           bwD2H, msD2H, bufSize / (1024 * 1024));

    cudaFree(d_buf);
    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    delete[] h_buf;
    PASS();
}

// ============================================================
// Main
// ============================================================

int main()
{
    printf("CUDA Verification Test: MetroEffects GPU vs CPU Comparison\n");
    printf("============================================================\n\n");

    // Check CUDA device availability
    int deviceCount = 0;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    if (err != cudaSuccess || deviceCount == 0) {
        printf("ERROR: No CUDA-capable device found.\n");
        printf("  cudaGetDeviceCount returned %d: %s\n",
               deviceCount, cudaGetErrorString(err));
        printf("This test requires a machine with an NVIDIA GPU.\n\n");
        return 1;
    }

    cudaDeviceProp props;
    cudaGetDeviceProperties(&props, 0);
    printf("Device: %s (SM %d.%d, %zu MB global mem)\n\n",
           props.name, props.major, props.minor,
           props.totalGlobalMem / (1024 * 1024));

    // ========================================
    // FilmGrain Tests
    // ========================================
    printf("--- FilmGrain ---\n");
    test_filmgrain_generate_grain_vs_cpu();
    test_filmgrain_generate_grain_zero_sharpness();
    test_filmgrain_generate_grain_max_sharpness();
    test_filmgrain_generate_min_grain_size();
    test_filmgrain_apply_luma_vs_cpu();
    test_filmgrain_apply_color_vs_cpu();
    test_filmgrain_generate_invalid_params();
    test_filmgrain_apply_invalid_params();

    // ========================================
    // Glow Tests
    // ========================================
    printf("\n--- Glow ---\n");
    test_glow_pipeline_vs_cpu();
    test_glow_threshold_edge_cases();
    test_glow_zero_radius();

    // ========================================
    // LensFlare Tests
    // ========================================
    printf("\n--- LensFlare ---\n");
    test_lensflare_pipeline_vs_cpu();
    test_lensflare_brightness_zero();
    test_lensflare_edge_params();

    // ========================================
    // Edge Cases
    // ========================================
    printf("\n--- Edge Cases ---\n");
    test_empty_rect();
    test_lensflare_empty_rect();
    test_all_black_image();
    test_all_white_image();

    // ========================================
    // Occupancy & Bandwidth
    // ========================================
    printf("\n--- Device Info, Occupancy & Bandwidth ---\n");
    test_device_info();
    measure_bandwidth_1920x1080();

    // ========================================
    // Summary
    // ========================================
    printf("\n============================================\n");
    printf("Results: %d/%d passed, %d failed\n",
           s_passCount, s_testCount, s_failCount);
    printf("============================================\n");

    return (s_failCount == 0) ? 0 : 1;
}
