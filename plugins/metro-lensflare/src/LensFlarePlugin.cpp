// Copyright (c) 2026 Metro Design. All rights reserved.
#include "metro/ofx/Plugin.hpp"
#include "metro/ofx/param/ParamManager.hpp"
#include "metro/gpu/Pipeline.hpp"

#include "LensFlareKernels.hpp"

#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cfloat>
#include <vector>

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

static const char *kParamBrightness       = "brightness";
static const char *kParamFlareSize        = "flareSize";
static const char *kParamGhostCount       = "ghostCount";
static const char *kParamAnamorphicStretch= "anamorphicStretch";
static const char *kParamChromaShift      = "chromaShift";
static const char *kParamHueTint          = "hueTint";
static const char *kParamMix              = "mix";

static float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

static float gaussian(float x, float sigma)
{
    return std::exp(-(x * x) / (2.0f * sigma * sigma));
}

static void hsvToRgb(float h, float s, float v, float &r, float &g, float &b)
{
    float c = v * s;
    float hp = h / 60.0f;
    float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    float m = v - c;
    int hi = static_cast<int>(hp) % 6;
    switch (hi) {
        case 0: r = c; g = x; b = 0.0f; break;
        case 1: r = x; g = c; b = 0.0f; break;
        case 2: r = 0.0f; g = c; b = x; break;
        case 3: r = 0.0f; g = x; b = c; break;
        case 4: r = x; g = 0.0f; b = c; break;
        case 5: r = c; g = 0.0f; b = x; break;
        default: r = 0.0f; g = 0.0f; b = 0.0f; break;
    }
    r += m; g += m; b += m;
}

static float luminance(const float *pixel)
{
    return 0.2126f * pixel[0] + 0.7152f * pixel[1] + 0.0722f * pixel[2];
}

struct Float2 { float x, y; };

static Float2 flareCenterFromSource(const float *src, int stride, int w, int h, int nc)
{
    Float2 center = { static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.5f };
    float maxLum = -1.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float lum = luminance(&src[y * stride + x * nc]);
            if (lum > maxLum) {
                maxLum = lum;
                center.x = static_cast<float>(x);
                center.y = static_cast<float>(y);
            }
        }
    }
    return center;
}

static float bilerp(const float *src, int stride, int w, int h, float fx, float fy, int ch, int nc)
{
    float cx = clampf(fx, 0.0f, static_cast<float>(w - 1));
    float cy = clampf(fy, 0.0f, static_cast<float>(h - 1));
    int ix = static_cast<int>(cx);
    int iy = static_cast<int>(cy);
    float dx = cx - static_cast<float>(ix);
    float dy = cy - static_cast<float>(iy);
    int ix1 = std::min(ix + 1, w - 1);
    int iy1 = std::min(iy + 1, h - 1);
    float p00 = src[iy * stride + ix * nc + ch];
    float p10 = src[iy * stride + ix1 * nc + ch];
    float p01 = src[iy1 * stride + ix * nc + ch];
    float p11 = src[iy1 * stride + ix1 * nc + ch];
    float top = p00 + dx * (p10 - p00);
    float bot = p01 + dx * (p11 - p01);
    return top + dy * (bot - top);
}

class LensFlarePlugin : public Plugin {
public:
    const char *identifier() const override { return "com.metrodesign.lensflare"; }
    const char *label() const override { return "Metro Lens Flare"; }
    const char *description() const override {
        return "Simulates optical lens artifacts including central flare, anamorphic streaks, ghost reflections, and chromatic shift.";
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
        prop->propSetString(props, kOfxImageEffectPropShortLabel, 0, "MetroLensF");
        prop->propSetString(props, kOfxImageEffectPropLongLabel, 0, "Metro Design Lens Flare");
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
        if (prop) {
            prop->propSetString(clipProps, kOfxImageEffectPropLabel, 0, "Source");
        }

        effect->clipDefine(descriptor, kClipOutput, &clipProps);
        if (prop) {
            prop->propSetString(clipProps, kOfxImageEffectPropLabel, 0, "Output");
        }

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
        auto setIntMin = [&](OfxPropertySetHandle p, int val) {
            if (prop) prop->propSetInt(p, kOfxParamPropIntMin, 0, val);
        };
        auto setIntMax = [&](OfxPropertySetHandle p, int val) {
            if (prop) prop->propSetInt(p, kOfxParamPropIntMax, 0, val);
        };
        auto setIntDefault = [&](OfxPropertySetHandle p, int val) {
            if (prop) prop->propSetInt(p, kOfxParamPropIntDefault, 0, val);
        };

        stat = defParam(kOfxParamTypeDouble, kParamBrightness, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Brightness");
        setHint(paramProps, "Overall lens flare intensity (0=off)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 5.0);
        setDoubleDefault(paramProps, 1.0);
        setIncrement(paramProps, 0.1);
        setDigits(paramProps, 2);

        stat = defParam(kOfxParamTypeDouble, kParamFlareSize, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Flare Size");
        setHint(paramProps, "Scale factor for flare and ghost element sizes");
        setDoubleMin(paramProps, 0.1);
        setDoubleMax(paramProps, 5.0);
        setDoubleDefault(paramProps, 1.0);
        setIncrement(paramProps, 0.1);
        setDigits(paramProps, 2);

        stat = defParam(kOfxParamTypeInteger, kParamGhostCount, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Ghost Count");
        setHint(paramProps, "Number of ghost reflection artifacts");
        setIntMin(paramProps, 1);
        setIntMax(paramProps, 10);
        setIntDefault(paramProps, 4);

        stat = defParam(kOfxParamTypeDouble, kParamAnamorphicStretch, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Anamorphic Stretch");
        setHint(paramProps, "Horizontal streak intensity (0=off, 1=full)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 0.3);
        setIncrement(paramProps, 0.05);
        setDigits(paramProps, 3);

        stat = defParam(kOfxParamTypeDouble, kParamChromaShift, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Chroma Shift");
        setHint(paramProps, "RGB channel separation for ghost elements");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 0.2);
        setIncrement(paramProps, 0.05);
        setDigits(paramProps, 3);

        stat = defParam(kOfxParamTypeDouble, kParamHueTint, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Hue Tint");
        setHint(paramProps, "Color tint for the flare (degrees)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 360.0);
        setDoubleDefault(paramProps, 0.0);
        setIncrement(paramProps, 1.0);
        setDigits(paramProps, 1);

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
        int srcW = srcBounds[2] - srcBounds[0];
        int srcH = srcBounds[3] - srcBounds[1];

        int dstRowBytes = 0;
        prop->propGetInt(dstImgProps, kOfxImagePropRowBytes, 0, &dstRowBytes);
        int dstBounds[4] = {0, 0, 0, 0};
        prop->propGetIntN(dstImgProps, kOfxImageEffectPropBounds, 4, dstBounds);
        int dstW = dstBounds[2] - dstBounds[0];
        int dstH = dstBounds[3] - dstBounds[1];

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
        auto getInt = [&](const char *name, int &v) -> OfxStatus {
            OfxParamSetHandle p;
            OfxStatus s = param->paramGetHandle(paramSet, name, &p, nullptr);
            if (s != kOfxStatOK) return s;
            return param->paramGetValue(p, 0, &v);
        };

        double brightness = 1.0, flareSize = 1.0, anamorphicStretch = 0.3;
        double chromaShift = 0.2, hueTint = 0.0, mix = 1.0;
        int ghostCount = 4;
        if (getDouble(kParamBrightness, brightness) != kOfxStatOK) brightness = 1.0;
        if (getDouble(kParamFlareSize, flareSize) != kOfxStatOK) flareSize = 1.0;
        if (getInt(kParamGhostCount, ghostCount) != kOfxStatOK) ghostCount = 4;
        if (getDouble(kParamAnamorphicStretch, anamorphicStretch) != kOfxStatOK) anamorphicStretch = 0.3;
        if (getDouble(kParamChromaShift, chromaShift) != kOfxStatOK) chromaShift = 0.2;
        if (getDouble(kParamHueTint, hueTint) != kOfxStatOK) hueTint = 0.0;
        if (getDouble(kParamMix, mix) != kOfxStatOK) mix = 1.0;

        const float *src = static_cast<const float *>(srcPtr);
        float *dst = static_cast<float *>(dstPtr);
        int srcStride = srcRowBytes / static_cast<int>(sizeof(float));
        int dstStride = dstRowBytes / static_cast<int>(sizeof(float));
        const int nc = 4;

        float brightF = static_cast<float>(brightness);
        float sizeF = static_cast<float>(flareSize);
        float anamorphicF = static_cast<float>(anamorphicStretch);
        float chromaF = static_cast<float>(chromaShift);
        float hueF = static_cast<float>(hueTint);
        float mixF = static_cast<float>(mix);

        float tintR, tintG, tintB;
        hsvToRgb(hueF, 1.0f, 1.0f, tintR, tintG, tintB);

        Float2 center = flareCenterFromSource(src, srcStride, srcW, srcH, nc);
        float cfx = center.x;
        float cfy = center.y;
        float imgCx = static_cast<float>(dstW) * 0.5f;
        float imgCy = static_cast<float>(dstH) * 0.5f;

#if METRO_HAVE_CUDA
        if (metro::gpu::probeDevices().type == metro::gpu::DeviceType::CUDA) {
            FlareParams fp;
            fp.brightness = brightF;
            fp.flareSize = sizeF;
            fp.ghostCount = ghostCount;
            fp.anamorphicStretch = anamorphicF;
            fp.chromaShift = chromaF;
            fp.hueTint = hueF;
            fp.mix = mixF;
            fp.centerX = cfx;
            fp.centerY = cfy;
            fp.imgCx = imgCx;
            fp.imgCy = imgCy;

            bool gpuOk = launchLensFlareGPU(src, dst,
                                            srcStride, dstStride,
                                            renderX1, renderY1, renderX2, renderY2,
                                            srcW, srcH, fp);
            if (gpuOk) {
                effect->imageClipReleaseImage(srcData);
                effect->imageClipReleaseImage(dstData);
                return kOfxStatOK;
            }
        }
#endif

        float axisDx = imgCx - cfx;
        float axisDy = imgCy - cfy;
        float axisLen = std::sqrt(axisDx * axisDx + axisDy * axisDy);
        if (axisLen < 0.001f) {
            axisDx = 1.0f; axisDy = 0.0f;
        } else {
            axisDx /= axisLen; axisDy /= axisLen;
        }

        for (int y = renderY1; y < renderY2; ++y) {
            for (int x = renderX1; x < renderX2; ++x) {
                float origR = src[y * srcStride + x * nc + 0];
                float origG = src[y * srcStride + x * nc + 1];
                float origB = src[y * srcStride + x * nc + 2];
                float origA = src[y * srcStride + x * nc + 3];

                float fx = static_cast<float>(x);
                float fy = static_cast<float>(y);
                float dx = fx - cfx;
                float dy = fy - cfy;
                float dist = std::sqrt(dx * dx + dy * dy);

                float flareR = 0.0f, flareG = 0.0f, flareB = 0.0f;

                if (brightF > 0.0f) {
                    float centralSigma = std::max(8.0f * sizeF, 1.0f);
                    float centralIntensity = brightF * gaussian(dist, centralSigma);
                    flareR += centralIntensity;
                    flareG += centralIntensity;
                    flareB += centralIntensity;

                    if (anamorphicF > 0.0f) {
                        float perpDist = std::abs(dx * (-axisDy) + dy * axisDx);
                        float streakSigma = std::max(4.0f * sizeF, 1.0f);
                        float streakIntensity = brightF * anamorphicF * gaussian(perpDist, streakSigma);
                        flareR += streakIntensity;
                        flareG += streakIntensity;
                        flareB += streakIntensity;
                    }

                    float ghostSpacing = std::max(30.0f * sizeF, 5.0f);
                    float chromaPx = chromaF * 6.0f * sizeF;
                    for (int gi = 0; gi < ghostCount; ++gi) {
                        float t = static_cast<float>(gi + 1) * 0.18f;
                        float gx = cfx + axisDx * ghostSpacing * t;
                        float gy = cfy + axisDy * ghostSpacing * t;
                        float gdx = fx - gx;
                        float gdy = fy - gy;
                        float gdist = std::sqrt(gdx * gdx + gdy * gdy);

                        float ghostSigma = std::max(10.0f * sizeF * (1.0f + t * 0.5f), 1.0f);
                        float ghostIntensity = brightF * gaussian(gdist, ghostSigma) * (0.7f - static_cast<float>(gi) * 0.06f);
                        if (ghostIntensity > 0.0f) {
                            float gr = ghostIntensity;
                            float gg = ghostIntensity;
                            float gb = ghostIntensity;

                            if (chromaF > 0.0f) {
                                float cr = bilerp(src, srcStride, srcW, srcH,
                                                  fx + chromaPx, fy, 0, nc);
                                float cg = bilerp(src, srcStride, srcW, srcH,
                                                  fx, fy, 1, nc);
                                float cb = bilerp(src, srcStride, srcW, srcH,
                                                  fx - chromaPx, fy, 2, nc);
                                float clum = 0.2126f * cr + 0.7152f * cg + 0.0722f * cb;
                                gr = ghostIntensity * (clum + (cr - clum) * chromaF);
                                gg = ghostIntensity * (clum + (cg - clum) * chromaF);
                                gb = ghostIntensity * (clum + (cb - clum) * chromaF);
                            }

                            float tintBlend = 0.3f;
                            flareR += gr * (1.0f - tintBlend) + gr * tintBlend * tintR;
                            flareG += gg * (1.0f - tintBlend) + gg * tintBlend * tintG;
                            flareB += gb * (1.0f - tintBlend) + gb * tintBlend * tintB;
                        }
                    }
                }

                float outR = origR + mixF * (flareR);
                float outG = origG + mixF * (flareG);
                float outB = origB + mixF * (flareB);

                dst[y * dstStride + x * nc + 0] = outR;
                dst[y * dstStride + x * nc + 1] = outG;
                dst[y * dstStride + x * nc + 2] = outB;
                dst[y * dstStride + x * nc + 3] = origA;
            }
        }

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

        double brightness = 0.0;
        getDouble(kParamBrightness, brightness);

        if (brightness == 0.0) {
            prop->propSetString(outArgs, kOfxImageEffectPropIsIdentityClip, 0, kClipSource);
            return kOfxStatOK;
        }
        return kOfxStatReplyDefault;
    }
};

static LensFlarePlugin s_lensflarePlugin;
static PluginRegistrar s_registrar(&s_lensflarePlugin);
