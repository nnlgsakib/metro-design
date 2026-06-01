#include "metro/ofx/Plugin.hpp"
#include "metro/ofx/param/ParamManager.hpp"
#include "metro/gpu/Pipeline.hpp"

#include "GlowKernels.hpp"

#include <cmath>
#include <cstring>
#include <algorithm>

using namespace metro::ofx;
using namespace metro::ofx::param;

#define kOfxPropTime                         "OfxPropTime"
#define kOfxImageEffectPropData              "OfxImageEffectPropData"
#define kOfxImageEffectPropBounds            "OfxImageEffectPropBounds"
#define kOfxImageEffectPropRegionOfDefinition "OfxImageEffectPropRegionOfDefinition"
#define kOfxImageEffectPropRenderWindow      "OfxImageEffectPropRenderWindow"
#define kOfxImageEffectPropRenderScale       "OfxImageEffectPropRenderScale"
#define kOfxImageEffectPropPixelDepth        "OfxImageEffectPropPixelDepth"
#define kOfxImageEffectPropComponents        "OfxImageEffectPropComponents"
#define kOfxImageEffectPropIsIdentityClip    "OfxImageEffectPropIsIdentityClip"
#define kOfxImageEffectPropSrcClip           "OfxImageEffectPropSrcClip"
#define kOfxImagePropRowBytes                "OfxImagePropRowBytes"
#define kOfxImageClipPropConnected           "OfxImageClipPropConnected"
#define kOfxBitDepthFloat                    "OfxBitDepthFloat"
#define kOfxImageComponentRGBA               "OfxImageComponentRGBA"

static const char *kClipSource = "Source";
static const char *kClipOutput = "Output";

static const char *kParamIntensity = "intensity";
static const char *kParamThreshold = "threshold";
static const char *kParamRadius   = "radius";
static const char *kParamGlowR    = "glowColorR";
static const char *kParamGlowG    = "glowColorG";
static const char *kParamGlowB    = "glowColorB";
static const char *kParamMix      = "mix";

static float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

static void boxBlurH(const float *src, float *dst, int w, int h, int nc, int radius)
{
    float inv = 1.0f / (2.0f * radius + 1.0f);
    for (int y = 0; y < h; ++y) {
        for (int c = 0; c < nc; ++c) {
            float sum = 0.0f;
            int stride = w * nc;
            for (int x = -radius; x <= radius; ++x) {
                int ix = std::max(0, std::min(w - 1, x));
                sum += src[y * stride + ix * nc + c];
            }
            dst[y * stride + 0 * nc + c] = sum * inv;
            for (int x = 1; x < w; ++x) {
                int prev = std::max(0, std::min(w - 1, x - radius - 1));
                int next = std::max(0, std::min(w - 1, x + radius));
                sum += src[y * stride + next * nc + c] - src[y * stride + prev * nc + c];
                dst[y * stride + x * nc + c] = sum * inv;
            }
        }
    }
}

static void boxBlurV(const float *src, float *dst, int w, int h, int nc, int radius)
{
    float inv = 1.0f / (2.0f * radius + 1.0f);
    int stride = w * nc;
    for (int x = 0; x < w; ++x) {
        for (int c = 0; c < nc; ++c) {
            float sum = 0.0f;
            for (int y = -radius; y <= radius; ++y) {
                int iy = std::max(0, std::min(h - 1, y));
                sum += src[iy * stride + x * nc + c];
            }
            dst[0 * stride + x * nc + c] = sum * inv;
            for (int y = 1; y < h; ++y) {
                int prev = std::max(0, std::min(h - 1, y - radius - 1));
                int next = std::max(0, std::min(h - 1, y + radius));
                sum += src[next * stride + x * nc + c] - src[prev * stride + x * nc + c];
                dst[y * stride + x * nc + c] = sum * inv;
            }
        }
    }
}

static void gaussianBlur(const float *src, float *tmp, float *dst, int w, int h, int nc, float radius)
{
    int r = static_cast<int>(std::ceil(radius));
    if (r < 1) {
        std::memcpy(dst, src, static_cast<size_t>(w) * h * nc * sizeof(float));
        return;
    }
    boxBlurH(src, tmp, w, h, nc, r);
    boxBlurV(tmp, dst, w, h, nc, r);
}

class GlowPlugin : public Plugin {
public:
    const char *identifier() const override { return "com.metrodesign.glow"; }
    const char *label() const override { return "Metro Glow"; }
    const char *description() const override {
        return "Bloom/glow effect with threshold, blur, and color tint control.";
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
        prop->propSetString(props, kOfxImageEffectPropShortLabel, 0, "MetroGlow");
        prop->propSetString(props, kOfxImageEffectPropLongLabel, 0, "Metro Design Glow");
        prop->propSetString(props, kOfxImageEffectPropGrouping, 0, pluginGrouping());
        prop->propSetString(props, kOfxImageEffectPropDescription, 0, description());

        const char *contexts[] = { kOfxImageEffectContextFilter, kOfxImageEffectContextGeneral, nullptr };
        prop->propSetStringN(props, kOfxImageEffectPropSupportedContexts, 2, contexts);

        return kOfxStatOK;
    }

    OfxStatus describeInContext(OfxImageEffectHandle descriptor, int contextIndex) override
    {
        (void)contextIndex;
        if (!hostAvailable() || !host().imageEffect() || !host().parameters())
            return kOfxStatErrBadHandle;

        OfxPropertySetHandle clipProps;
        OfxPropertySetHandle paramProps;
        const OfxImageEffectSuiteV1 *effect = host().imageEffect();
        const OfxPropertySuiteV1 *prop = host().properties();
        const OfxParamSuiteV1 *param = host().parameters();

        effect->clipDefine(descriptor, kClipSource, &clipProps);
        if (prop) prop->propSetString(clipProps, kOfxImageEffectPropLabel, 0, "Source");

        effect->clipDefine(descriptor, kClipOutput, &clipProps);
        if (prop) prop->propSetString(clipProps, kOfxImageEffectPropLabel, 0, "Output");

        OfxParamSetHandle paramSet;
        OfxStatus stat = effect->getParamSet(descriptor, &paramSet);
        if (stat != kOfxStatOK) return stat;

        auto defParam = [&](const char *type, const char *name, OfxPropertySetHandle &outProps) -> OfxStatus {
            return param->paramDefine(paramSet, type, name, &outProps);
        };
        auto setLabel = [&](OfxPropertySetHandle p, const char *val) {
            if (prop) prop->propSetString(p, kOfxParamPropLabel, 0, val);
        };
        auto setHint = [&](OfxPropertySetHandle p, const char *val) {
            if (prop) prop->propSetString(p, kOfxParamPropHint, 0, val);
        };
        auto setDoubleMin = [&](OfxPropertySetHandle p, double val) {
            if (prop) prop->propSetDouble(p, kOfxParamPropDoubleMin, 0, val);
        };
        auto setDoubleMax = [&](OfxPropertySetHandle p, double val) {
            if (prop) prop->propSetDouble(p, kOfxParamPropDoubleMax, 0, val);
        };
        auto setDoubleDefault = [&](OfxPropertySetHandle p, double val) {
            if (prop) prop->propSetDouble(p, kOfxParamPropDoubleDefault, 0, val);
        };
        auto setIncrement = [&](OfxPropertySetHandle p, double val) {
            if (prop) prop->propSetDouble(p, kOfxParamPropIncrement, 0, val);
        };
        auto setDigits = [&](OfxPropertySetHandle p, int val) {
            if (prop) prop->propSetInt(p, kOfxParamPropDigits, 0, val);
        };

        stat = defParam(kOfxParamTypeDouble, kParamIntensity, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Intensity");
        setHint(paramProps, "Strength of the glow effect");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 0.5);
        setIncrement(paramProps, 0.05);
        setDigits(paramProps, 3);

        stat = defParam(kOfxParamTypeDouble, kParamThreshold, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Threshold");
        setHint(paramProps, "Luminance threshold for glow extraction");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 0.5);
        setIncrement(paramProps, 0.05);
        setDigits(paramProps, 3);

        stat = defParam(kOfxParamTypeDouble, kParamRadius, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Radius");
        setHint(paramProps, "Glow spread radius in pixels");
        setDoubleMin(paramProps, 1.0);
        setDoubleMax(paramProps, 100.0);
        setDoubleDefault(paramProps, 10.0);
        setIncrement(paramProps, 1.0);
        setDigits(paramProps, 1);

        stat = defParam(kOfxParamTypeDouble, kParamGlowR, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Glow Color Red");
        setHint(paramProps, "Red tint of the glow (0-1)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 1.0);
        setIncrement(paramProps, 0.05);
        setDigits(paramProps, 3);

        stat = defParam(kOfxParamTypeDouble, kParamGlowG, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Glow Color Green");
        setHint(paramProps, "Green tint of the glow (0-1)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 1.0);
        setIncrement(paramProps, 0.05);
        setDigits(paramProps, 3);

        stat = defParam(kOfxParamTypeDouble, kParamGlowB, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Glow Color Blue");
        setHint(paramProps, "Blue tint of the glow (0-1)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 1.0);
        setIncrement(paramProps, 0.05);
        setDigits(paramProps, 3);

        stat = defParam(kOfxParamTypeDouble, kParamMix, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Mix");
        setHint(paramProps, "Blend between original and effect (0=original, 1=full effect)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 1.0);
        setIncrement(paramProps, 0.05);
        setDigits(paramProps, 3);

        return kOfxStatOK;
    }

    OfxStatus render(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) override
    {
        (void)outArgs;
        if (!hostAvailable() || !host().imageEffect() || !host().properties() || !host().parameters())
            return kOfxStatErrBadHandle;

        const OfxImageEffectSuiteV1 *effect = host().imageEffect();
        const OfxPropertySuiteV1 *prop = host().properties();
        const OfxParamSuiteV1 *param = host().parameters();

        double time = 0.0;
        if (prop->propGetDouble(inArgs, kOfxPropTime, 0, &time) != kOfxStatOK) {
            time = 0.0;
        }

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
        prop->propGetInt(srcImgProps, kOfxImagePropRowBytes, 0, &srcRowBytes);
        int srcBounds[4] = {0, 0, 0, 0};
        prop->propGetIntN(srcImgProps, kOfxImageEffectPropBounds, 4, srcBounds);

        int dstRowBytes = 0;
        prop->propGetInt(dstImgProps, kOfxImagePropRowBytes, 0, &dstRowBytes);
        int dstBounds[4] = {0, 0, 0, 0};
        prop->propGetIntN(dstImgProps, kOfxImageEffectPropBounds, 4, dstBounds);

        void *srcPtr = nullptr;
        void *dstPtr = nullptr;
        prop->propGetPointer(srcImgProps, kOfxImageEffectPropData, 0, &srcPtr);
        prop->propGetPointer(dstImgProps, kOfxImageEffectPropData, 0, &dstPtr);
        if (!srcPtr || !dstPtr) {
            effect->imageClipReleaseImage(srcData);
            effect->imageClipReleaseImage(dstData);
            return kOfxStatErrBadHandle;
        }

        int renderWindow[4] = {0, 0, 0, 0};
        prop->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, renderWindow);
        int renderX1 = renderWindow[0], renderY1 = renderWindow[1];
        int renderX2 = renderWindow[2], renderY2 = renderWindow[3];

        OfxParamSetHandle paramSet;
        stat = effect->getParamSet(instance, &paramSet);
        if (stat != kOfxStatOK) {
            effect->imageClipReleaseImage(srcData);
            effect->imageClipReleaseImage(dstData);
            return stat;
        }

        auto getDouble = [&](const char *name, double &v) -> OfxStatus {
            OfxParamSetHandle p;
            OfxStatus s = param->paramGetHandle(paramSet, name, &p, nullptr);
            if (s != kOfxStatOK) return s;
            return param->paramGetValue(p, 0, &v);
        };

        double intensity, threshold, radius, glowR, glowG, glowB, mix;
        if (getDouble(kParamIntensity, intensity) != kOfxStatOK) intensity = 0.5;
        if (getDouble(kParamThreshold, threshold) != kOfxStatOK) threshold = 0.5;
        if (getDouble(kParamRadius, radius) != kOfxStatOK) radius = 10.0;
        if (getDouble(kParamGlowR, glowR) != kOfxStatOK) glowR = 1.0;
        if (getDouble(kParamGlowG, glowG) != kOfxStatOK) glowG = 1.0;
        if (getDouble(kParamGlowB, glowB) != kOfxStatOK) glowB = 1.0;
        if (getDouble(kParamMix, mix) != kOfxStatOK) mix = 1.0;

        const float *src = static_cast<const float *>(srcPtr);
        float *dst = static_cast<float *>(dstPtr);
        int srcStride = srcRowBytes / static_cast<int>(sizeof(float));

#if METRO_HAVE_CUDA
        if (metro::gpu::probeDevices().type == metro::gpu::DeviceType::CUDA) {
            bool gpuOk = launchGlowGPU(src, dst, srcStride, dstStride,
                                       renderX1, renderY1, renderX2, renderY2,
                                       intensityF, thresholdF, radiusF,
                                       glowRF, glowGF, glowBF, mixF);
            if (gpuOk) {
                delete[] tmp;
                delete[] blurred;
                effect->imageClipReleaseImage(srcData);
                effect->imageClipReleaseImage(dstData);
                return kOfxStatOK;
            }
        }
#endif
        int dstStride = dstRowBytes / static_cast<int>(sizeof(float));
        const int nc = 4;
        float intensityF = clampf(static_cast<float>(intensity), 0.0f, 1.0f);
        float thresholdF = clampf(static_cast<float>(threshold), 0.0f, 1.0f);
        float radiusF = std::max(1.0f, static_cast<float>(radius));
        float glowRF = clampf(static_cast<float>(glowR), 0.0f, 1.0f);
        float glowGF = clampf(static_cast<float>(glowG), 0.0f, 1.0f);
        float glowBF = clampf(static_cast<float>(glowB), 0.0f, 1.0f);
        float mixF = clampf(static_cast<float>(mix), 0.0f, 1.0f);

        int rw = renderX2 - renderX1;
        int rh = renderY2 - renderY1;

        float *tmp = nullptr;
        float *blurred = nullptr;
        bool useTemp = (rw > 0 && rh > 0);

        if (useTemp) {
            tmp = new (std::nothrow) float[rw * rh * nc];
            blurred = new (std::nothrow) float[rw * rh * nc];
            if (!tmp || !blurred) {
                delete[] tmp;
                delete[] blurred;
                effect->imageClipReleaseImage(srcData);
                effect->imageClipReleaseImage(dstData);
                return kOfxStatErrMemory;
            }
        }

        for (int y = renderY1; y < renderY2; ++y) {
            for (int x = renderX1; x < renderX2; ++x) {
                int si = y * srcStride + x * nc;
                float luma = 0.2126f * src[si + 0] + 0.7152f * src[si + 1] + 0.0722f * src[si + 2];
                float bright = std::max(0.0f, luma - thresholdF) / (1.0f - thresholdF + 1e-6f);
                int ti = (y - renderY1) * rw * nc + (x - renderX1) * nc;
                blurred[ti + 0] = src[si + 0] * bright;
                blurred[ti + 1] = src[si + 1] * bright;
                blurred[ti + 2] = src[si + 2] * bright;
                blurred[ti + 3] = src[si + 3];
            }
        }

        if (radiusF > 1.0f) {
            gaussianBlur(blurred, tmp, blurred, rw, rh, nc, radiusF);
        }

        for (int y = renderY1; y < renderY2; ++y) {
            for (int x = renderX1; x < renderX2; ++x) {
                int di = y * dstStride + x * nc;
                int bi = (y - renderY1) * rw * nc + (x - renderX1) * nc;
                float origR = src[y * srcStride + x * nc + 0];
                float origG = src[y * srcStride + x * nc + 1];
                float origB = src[y * srcStride + x * nc + 2];
                float origA = src[y * srcStride + x * nc + 3];

                float glowR_ = blurred[bi + 0] * glowRF * intensityF;
                float glowG_ = blurred[bi + 1] * glowGF * intensityF;
                float glowB_ = blurred[bi + 2] * glowBF * intensityF;

                dst[di + 0] = origR + mixF * (glowR_);
                dst[di + 1] = origG + mixF * (glowG_);
                dst[di + 2] = origB + mixF * (glowB_);
                dst[di + 3] = origA;
            }
        }

        delete[] tmp;
        delete[] blurred;

        effect->imageClipReleaseImage(srcData);
        effect->imageClipReleaseImage(dstData);

        return kOfxStatOK;
    }

    OfxStatus isIdentity(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) override
    {
        (void)inArgs;
        if (!hostAvailable() || !host().imageEffect() || !host().parameters() || !host().properties())
            return kOfxStatReplyDefault;

        const OfxPropertySuiteV1 *prop = host().properties();
        const OfxImageEffectSuiteV1 *effect = host().imageEffect();
        const OfxParamSuiteV1 *param = host().parameters();

        OfxParamSetHandle paramSet;
        OfxStatus stat = effect->getParamSet(instance, &paramSet);
        if (stat != kOfxStatOK) return kOfxStatReplyDefault;

        auto getDouble = [&](const char *name, double &v) -> bool {
            OfxParamSetHandle p;
            if (param->paramGetHandle(paramSet, name, &p, nullptr) != kOfxStatOK) return false;
            return param->paramGetValue(p, 0, &v) == kOfxStatOK;
        };

        double intensity = 0.0, mix = 0.0;
        getDouble(kParamIntensity, intensity);
        getDouble(kParamMix, mix);

        if (intensity == 0.0 || mix == 0.0) {
            prop->propSetString(outArgs, kOfxImageEffectPropIsIdentityClip, 0, kClipSource);
            return kOfxStatOK;
        }
        return kOfxStatReplyDefault;
    }
};

static GlowPlugin s_glowPlugin;
static PluginRegistrar s_registrar(&s_glowPlugin);
