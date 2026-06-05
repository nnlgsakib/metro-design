#include "metro/ofx/Plugin.hpp"
#include "metro/ofx/param/ParamManager.hpp"
#include "metro/gpu/Pipeline.hpp"
#include "FilmGrainKernels.cuh"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdio>

using namespace metro::ofx;
using namespace metro::ofx::param;

static const char *kClipSource = "Source";
static const char *kClipOutput = "Output";

static const char *kParamAmount    = "amount";
static const char *kParamSize      = "size";
static const char *kParamSharpness = "sharpness";
static const char *kParamGrainType = "grainType";
static const char *kParamSeed      = "seed";
static const char *kParamMix       = "mix";

#define kOfxImageEffectPropIsIdentityClip "OfxImageEffectPropIsIdentityClip"

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
            float shaped = powf(fabsf(centered), p);
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

class FilmGrainPlugin : public Plugin {
public:
    const char *identifier() const override { return "com.metrodesign.filmgrain"; }
    const char *label() const override { return "Metro Film Grain"; }
    const char *description() const override {
        return "Analog film grain simulation with CUDA-accelerated texture generation.";
    }
    const char *versionString() const override { return "1.0.0"; }
    const char *pluginGrouping() const override { return "Metro Design"; }

    OfxStatus describe(OfxImageEffectHandle descriptor) override
    {
        if (!hostAvailable() || !host().properties()) return kOfxStatErrBadHandle;

        OfxPropertySetHandle props;
        OfxStatus stat = host().imageEffect()->getPropertySet(descriptor, &props);
        if (stat != kOfxStatOK) return stat;

        const OfxPropertySuiteV1 *prop = host().properties();

        prop->propSetString(props, kOfxImageEffectPropLabel, 0, label());
        prop->propSetString(props, kOfxImageEffectPropShortLabel, 0, "MetroGrain");
        prop->propSetString(props, kOfxImageEffectPropLongLabel, 0, "Metro Design Film Grain");
        prop->propSetString(props, kOfxImageEffectPropGrouping, 0, pluginGrouping());
        prop->propSetString(props, kOfxImageEffectPropDescription, 0, description());

        const char *contexts[] = { kOfxImageEffectContextFilter, kOfxImageEffectContextGeneral, nullptr };
        prop->propSetStringN(props, kOfxImageEffectPropSupportedContexts, 2, contexts);

        return kOfxStatOK;
    }

    OfxStatus describeInContext(OfxImageEffectHandle descriptor, int) override
    {
        if (!hostAvailable() || !host().imageEffect() || !host().parameters())
            return kOfxStatErrBadHandle;

        OfxPropertySetHandle clipProps, paramProps;
        const auto *effect = host().imageEffect();
        const auto *prop = host().properties();
        const auto *param = host().parameters();

        effect->clipDefine(descriptor, kClipSource, &clipProps);
        if (prop) prop->propSetString(clipProps, kOfxImageEffectPropLabel, 0, "Source");

        effect->clipDefine(descriptor, kClipOutput, &clipProps);
        if (prop) prop->propSetString(clipProps, kOfxImageEffectPropLabel, 0, "Output");

        OfxParamSetHandle paramSet;
        OfxStatus stat = effect->getParamSet(descriptor, &paramSet);
        if (stat != kOfxStatOK) return stat;

        auto defP = [&](const char *t, const char *n, auto &p) {
            return param->paramDefine(paramSet, t, n, &p);
        };
        auto lbl = [&](auto p, const char *v) { if (prop) prop->propSetString(p, kOfxParamPropLabel, 0, v); };
        auto hint = [&](auto p, const char *v) { if (prop) prop->propSetString(p, kOfxParamPropHint, 0, v); };
        auto mn = [&](auto p, double v) { if (prop) prop->propSetDouble(p, kOfxParamPropDoubleMin, 0, v); };
        auto mx = [&](auto p, double v) { if (prop) prop->propSetDouble(p, kOfxParamPropDoubleMax, 0, v); };
        auto defV = [&](auto p, double v) { if (prop) prop->propSetDouble(p, kOfxParamPropDoubleDefault, 0, v); };
        auto inc = [&](auto p, double v) { if (prop) prop->propSetDouble(p, kOfxParamPropIncrement, 0, v); };
        auto dig = [&](auto p, int v) { if (prop) prop->propSetInt(p, kOfxParamPropDigits, 0, v); };
        auto defI = [&](auto p, int v) { if (prop) prop->propSetInt(p, kOfxParamPropIntDefault, 0, v); };
        auto iMin = [&](auto p, int v) { if (prop) prop->propSetInt(p, kOfxParamPropIntMin, 0, v); };
        auto iMax = [&](auto p, int v) { if (prop) prop->propSetInt(p, kOfxParamPropIntMax, 0, v); };

        stat = defP(kOfxParamTypeDouble, kParamAmount, paramProps); if (stat != kOfxStatOK) return stat;
        lbl(paramProps, "Amount"); hint(paramProps, "Grain intensity");
        mn(paramProps, 0.0); mx(paramProps, 1.0); defV(paramProps, 0.3);
        inc(paramProps, 0.05); dig(paramProps, 3);

        stat = defP(kOfxParamTypeDouble, kParamSize, paramProps); if (stat != kOfxStatOK) return stat;
        lbl(paramProps, "Size"); hint(paramProps, "Grain particle size");
        mn(paramProps, 0.5); mx(paramProps, 16.0); defV(paramProps, 3.0);
        inc(paramProps, 0.5); dig(paramProps, 1);

        stat = defP(kOfxParamTypeDouble, kParamSharpness, paramProps); if (stat != kOfxStatOK) return stat;
        lbl(paramProps, "Sharpness"); hint(paramProps, "Grain edge definition");
        mn(paramProps, 0.0); mx(paramProps, 1.0); defV(paramProps, 0.5);
        inc(paramProps, 0.05); dig(paramProps, 3);

        stat = defP(kOfxParamTypeChoice, kParamGrainType, paramProps); if (stat != kOfxStatOK) return stat;
        lbl(paramProps, "Grain Type"); hint(paramProps, "Luminance or color grain");
        if (prop) {
            const char *opts[] = { "Luminance", "Color", nullptr };
            prop->propSetStringN(paramProps, kOfxParamPropChoiceOption, 2, opts);
        }

        stat = defP(kOfxParamTypeInteger, kParamSeed, paramProps); if (stat != kOfxStatOK) return stat;
        lbl(paramProps, "Seed"); hint(paramProps, "Random seed for grain pattern");
        iMin(paramProps, 0); iMax(paramProps, 9999); defI(paramProps, 1234);

        stat = defP(kOfxParamTypeDouble, kParamMix, paramProps); if (stat != kOfxStatOK) return stat;
        lbl(paramProps, "Mix"); hint(paramProps, "Blend between original and effect");
        mn(paramProps, 0.0); mx(paramProps, 1.0); defV(paramProps, 1.0);
        inc(paramProps, 0.05); dig(paramProps, 3);

        return kOfxStatOK;
    }

    OfxStatus render(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) override
    {
        (void)outArgs;
        if (!hostAvailable() || !host().imageEffect() || !host().properties() || !host().parameters())
            return kOfxStatErrBadHandle;

        const auto *effect = host().imageEffect();
        const auto *prop = host().properties();
        const auto *param = host().parameters();

        double time = 0.0;
        prop->propGetDouble(inArgs, "OfxPropTime", 0, &time);

        OfxImageEffectHandle srcClip = nullptr;
        OfxPropertySetHandle srcClipProps = nullptr;
        OfxImageEffectHandle dstClip = nullptr;
        OfxPropertySetHandle dstClipProps = nullptr;

        OfxStatus stat = effect->clipGetHandle(instance, kClipSource, &srcClip, &srcClipProps);
        if (stat != kOfxStatOK || !srcClip) return kOfxStatErrBadHandle;

        stat = effect->clipGetHandle(instance, kClipOutput, &dstClip, &dstClipProps);
        if (stat != kOfxStatOK || !dstClip) return kOfxStatErrBadHandle;

        OfxPropertySetHandle srcImgProps = nullptr;
        OfxPropertySetHandle srcData = nullptr;
        OfxPropertySetHandle dstImgProps = nullptr;
        OfxPropertySetHandle dstData = nullptr;

        stat = effect->imageClipGetImage(srcClip, time, nullptr, &srcImgProps, &srcData);
        if (stat != kOfxStatOK || !srcImgProps || !srcData) return kOfxStatErrBadHandle;

        stat = effect->imageClipGetImage(dstClip, time, nullptr, &dstImgProps, &dstData);
        if (stat != kOfxStatOK || !dstImgProps || !dstData) {
            effect->imageClipReleaseImage(srcData);
            return kOfxStatErrBadHandle;
        }

        int srcRowBytes = 0;
        prop->propGetInt(srcImgProps, "OfxImagePropRowBytes", 0, &srcRowBytes);

        int dstRowBytes = 0;
        prop->propGetInt(dstImgProps, "OfxImagePropRowBytes", 0, &dstRowBytes);

        void *srcPtr = nullptr;
        void *dstPtr = nullptr;
        prop->propGetPointer(srcImgProps, "OfxImageEffectPropData", 0, &srcPtr);
        prop->propGetPointer(dstImgProps, "OfxImageEffectPropData", 0, &dstPtr);

        if (!srcPtr || !dstPtr) {
            effect->imageClipReleaseImage(srcData);
            effect->imageClipReleaseImage(dstData);
            return kOfxStatErrBadHandle;
        }

        int renderWindow[4] = {0, 0, 0, 0};
        prop->propGetIntN(inArgs, "OfxImageEffectPropRenderWindow", 4, renderWindow);
        int rx1 = renderWindow[0], ry1 = renderWindow[1];
        int rx2 = renderWindow[2], ry2 = renderWindow[3];
        int rw = rx2 - rx1;
        int rh = ry2 - ry1;

        OfxParamSetHandle paramSet;
        stat = effect->getParamSet(instance, &paramSet);
        if (stat != kOfxStatOK) {
            effect->imageClipReleaseImage(srcData);
            effect->imageClipReleaseImage(dstData);
            return stat;
        }

        auto getDouble = [&](const char *name, double &v) -> bool {
            OfxParamSetHandle h;
            if (param->paramGetHandle(paramSet, name, &h, nullptr) != kOfxStatOK) return false;
            return param->paramGetValue(h, 0, &v) == kOfxStatOK;
        };
        auto getInt = [&](const char *name, int &v) -> bool {
            OfxParamSetHandle h;
            if (param->paramGetHandle(paramSet, name, &h, nullptr) != kOfxStatOK) return false;
            return param->paramGetValue(h, 0, &v) == kOfxStatOK;
        };

        double amount = 0.3, size = 3.0, sharpness = 0.5, mix = 1.0;
        int grainType = 0, seed = 1234;
        getDouble(kParamAmount, amount);
        getDouble(kParamSize, size);
        getDouble(kParamSharpness, sharpness);
        getInt(kParamGrainType, grainType);
        getInt(kParamSeed, seed);
        getDouble(kParamMix, mix);

        const float *src = static_cast<const float *>(srcPtr);
        float *dst = static_cast<float *>(dstPtr);
        int srcStride = srcRowBytes / static_cast<int>(sizeof(float));
        int dstStride = dstRowBytes / static_cast<int>(sizeof(float));

        // Handle identity case (amount == 0) by copying source to dest if they differ
        if (amount == 0.0 || mix == 0.0) {
            if (srcPtr != dstPtr) {
                for (int y = ry1; y < ry2; ++y)
                    std::memcpy(dst + y * dstStride + rx1 * 4,
                                src + y * srcStride + rx1 * 4,
                                static_cast<size_t>(rw) * 4 * sizeof(float));
            }
            effect->imageClipReleaseImage(srcData);
            effect->imageClipReleaseImage(dstData);
            return kOfxStatOK;
        }

        float fAmount = static_cast<float>(amount);
        float fSize = static_cast<float>(size);
        float fSharpness = static_cast<float>(sharpness);
        float fMix = static_cast<float>(mix);

        // Allocate staging buffer (tightly packed RGBA render window) and grain buffer
        size_t tightStride = static_cast<size_t>(rw) * 4;
        size_t tightBytes = tightStride * static_cast<size_t>(rh) * sizeof(float);
        size_t grainBytes = static_cast<size_t>(rw) * static_cast<size_t>(rh) * sizeof(float);
        float* staging = static_cast<float*>(std::malloc(tightBytes));
        float* grain = static_cast<float*>(std::malloc(grainBytes));
        if (!staging || !grain) {
            std::free(staging); std::free(grain);
            effect->imageClipReleaseImage(srcData);
            effect->imageClipReleaseImage(dstData);
            return kOfxStatErrBadHandle;
        }

        // Copy render window from OFX buffer into tight staging buffer
        for (int y = 0; y < rh; ++y)
            std::memcpy(staging + y * tightStride,
                        src + (ry1 + y) * srcStride + rx1 * 4,
                        tightStride * sizeof(float));

#if METRO_HAVE_CUDA
        // --- GPU path ---
        {
            metro::gpu::DeviceInfo info = metro::gpu::probeDevices();
            bool gpuOk = (info.type == metro::gpu::DeviceType::CUDA && info.deviceCount > 0);

            metro::gpu::Buffer stagingBuf;
            metro::gpu::Buffer grainBuf;
            if (gpuOk) {
                gpuOk = stagingBuf.allocate(tightBytes) && grainBuf.allocate(grainBytes);
            }

            if (gpuOk) {
                gpuOk = stagingBuf.upload(staging, tightBytes);
            }

            if (gpuOk) {
                gpuOk = launchGenerateGrain(
                    static_cast<float*>(grainBuf.devicePtr()),
                    rw, rh, fSize, fSharpness,
                    static_cast<unsigned int>(seed));
            }

            if (gpuOk) {
                gpuOk = launchApplyGrain(
                    static_cast<const float*>(stagingBuf.devicePtr()),
                    static_cast<float*>(stagingBuf.devicePtr()),
                    static_cast<const float*>(grainBuf.devicePtr()),
                    rw, rh,
                    static_cast<int>(tightStride),
                    static_cast<int>(tightStride),
                    fAmount, fMix, grainType,
                    static_cast<unsigned int>(seed));
            }

            if (gpuOk) {
                gpuOk = stagingBuf.download(staging, tightBytes);
            }

            if (gpuOk) {
                // Copy staging back to OFX output buffer
                for (int y = 0; y < rh; ++y)
                    std::memcpy(dst + (ry1 + y) * dstStride + rx1 * 4,
                                staging + y * tightStride,
                                tightStride * sizeof(float));

                std::free(staging); std::free(grain);
                effect->imageClipReleaseImage(srcData);
                effect->imageClipReleaseImage(dstData);
                return kOfxStatOK;
            }

            if (info.deviceCount > 0) {
                fprintf(stderr, "FilmGrain: GPU path failed, falling back to CPU\n");
            }
        }
#endif

        // --- CPU path ---
        generateGrainCPU(grain, rw, rh, fSize, fSharpness, static_cast<unsigned int>(seed));

        applyGrainCPU(staging, staging, grain, rw, rh,
                      static_cast<int>(tightStride), static_cast<int>(tightStride),
                      fAmount, fMix, grainType, static_cast<unsigned int>(seed));

        // Copy staging back to OFX output buffer
        for (int y = 0; y < rh; ++y)
            std::memcpy(dst + (ry1 + y) * dstStride + rx1 * 4,
                        staging + y * tightStride,
                        tightStride * sizeof(float));

        std::free(grain);
        std::free(staging);

        effect->imageClipReleaseImage(srcData);
        effect->imageClipReleaseImage(dstData);

        return kOfxStatOK;
    }

    OfxStatus isIdentity(OfxImageEffectHandle instance, OfxPropertySetHandle, OfxPropertySetHandle outArgs) override
    {
        if (!hostAvailable() || !host().properties() || !host().parameters() || !host().imageEffect())
            return kOfxStatReplyDefault;

        auto *prop = host().properties();
        auto *param = host().parameters();
        auto *effect = host().imageEffect();

        OfxParamSetHandle p;
        if (effect->getParamSet(instance, &p) != kOfxStatOK) return kOfxStatReplyDefault;

        OfxParamSetHandle h;
        double amount = 0.3, mix = 1.0;
        if (param->paramGetHandle(p, kParamAmount, &h, nullptr) == kOfxStatOK)
            param->paramGetValue(h, 0, &amount);
        if (param->paramGetHandle(p, kParamMix, &h, nullptr) == kOfxStatOK)
            param->paramGetValue(h, 0, &mix);

        if (amount == 0.0 || mix == 0.0) {
            prop->propSetString(outArgs, kOfxImageEffectPropIsIdentityClip, 0, kClipSource);
            return kOfxStatOK;
        }
        return kOfxStatReplyDefault;
    }
};

static FilmGrainPlugin s_plugin;
static PluginRegistrar s_registrar(&s_plugin);
