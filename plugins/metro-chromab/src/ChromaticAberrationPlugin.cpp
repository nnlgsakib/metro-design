#include "metro/ofx/Plugin.hpp"
#include "metro/ofx/param/ParamManager.hpp"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cfloat>

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

static const char *kParamRedShift      = "redShift";
static const char *kParamGreenShift    = "greenShift";
static const char *kParamBlueShift     = "blueShift";
static const char *kParamRadialFalloff = "radialFalloff";
static const char *kParamStretchAngle  = "stretchAngle";
static const char *kParamMix           = "mix";

static bool isIdentityCheck(double rX, double rY, double gX, double gY, double bX, double bY)
{
    return rX == 0.0 && rY == 0.0 && gX == 0.0 && gY == 0.0 && bX == 0.0 && bY == 0.0;
}

static float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
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

class ChromaticAberrationPlugin : public Plugin {
public:
    const char *identifier() const override { return "com.metrodesign.chromaticaberration"; }
    const char *label() const override { return "Metro Chromatic Aberration"; }
    const char *description() const override {
        return "Per-channel chromatic displacement with radial falloff and stretch direction control.";
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
        prop->propSetString(props, kOfxImageEffectPropShortLabel, 0, "MetroChromaB");
        prop->propSetString(props, kOfxImageEffectPropLongLabel, 0, "Metro Design Chromatic Aberration");
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

        stat = defParam(kOfxParamTypeDouble2D, kParamRedShift, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Red Shift");
        setHint(paramProps, "Red channel displacement in pixels");
        setDoubleMin(paramProps, -100.0);
        setDoubleMax(paramProps, 100.0);
        setDoubleDefault(paramProps, 0.0);

        stat = defParam(kOfxParamTypeDouble2D, kParamGreenShift, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Green Shift");
        setHint(paramProps, "Green channel displacement in pixels");
        setDoubleMin(paramProps, -100.0);
        setDoubleMax(paramProps, 100.0);
        setDoubleDefault(paramProps, 0.0);

        stat = defParam(kOfxParamTypeDouble2D, kParamBlueShift, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Blue Shift");
        setHint(paramProps, "Blue channel displacement in pixels");
        setDoubleMin(paramProps, -100.0);
        setDoubleMax(paramProps, 100.0);
        setDoubleDefault(paramProps, 0.0);

        stat = defParam(kOfxParamTypeDouble, kParamRadialFalloff, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Radial Falloff");
        setHint(paramProps, "How displacement scales with distance from center (0=uniform, 1=full radial)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 0.0);
        setIncrement(paramProps, 0.05);
        setDigits(paramProps, 3);

        stat = defParam(kOfxParamTypeDouble, kParamStretchAngle, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Stretch Angle");
        setHint(paramProps, "Rotation angle for chromatic displacement direction (degrees)");
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

        auto getDouble2D = [&](const char *name, double &x, double &y) -> OfxStatus {
            OfxParamSetHandle p;
            OfxStatus s = param->paramGetHandle(paramSet, name, &p, nullptr);
            if (s != kOfxStatOK) return s;
            return param->paramGetValue(p, 0, &x, 1, &y);
        };
        auto getDouble = [&](const char *name, double &v) -> OfxStatus {
            OfxParamSetHandle p;
            OfxStatus s = param->paramGetHandle(paramSet, name, &p, nullptr);
            if (s != kOfxStatOK) return s;
            return param->paramGetValue(p, 0, &v);
        };

        double rX, rY, gX, gY, bX, bY;
        double radialFalloff, stretchAngle, mix;
        if (getDouble2D(kParamRedShift, rX, rY) != kOfxStatOK)   { rX = 0.0; rY = 0.0; }
        if (getDouble2D(kParamGreenShift, gX, gY) != kOfxStatOK) { gX = 0.0; gY = 0.0; }
        if (getDouble2D(kParamBlueShift, bX, bY) != kOfxStatOK)  { bX = 0.0; bY = 0.0; }
        if (getDouble(kParamRadialFalloff, radialFalloff) != kOfxStatOK) radialFalloff = 0.0;
        if (getDouble(kParamStretchAngle, stretchAngle) != kOfxStatOK) stretchAngle = 0.0;
        if (getDouble(kParamMix, mix) != kOfxStatOK) mix = 1.0;

        const float *src = static_cast<const float *>(srcPtr);
        float *dst = static_cast<float *>(dstPtr);
        int srcStride = srcRowBytes / static_cast<int>(sizeof(float));
        int dstStride = dstRowBytes / static_cast<int>(sizeof(float));
        const int nc = 4;
        float wf = static_cast<float>(dstW);
        float hf = static_cast<float>(dstH);
        float rFalloff = static_cast<float>(radialFalloff);
        float angleRad = static_cast<float>(stretchAngle) * 3.14159265358979323846f / 180.0f;
        float cosA = std::cos(angleRad);
        float sinA = std::sin(angleRad);
        float mixF = static_cast<float>(mix);

        for (int y = renderY1; y < renderY2; ++y) {
            for (int x = renderX1; x < renderX2; ++x) {
                float cx = (static_cast<float>(x) / wf) * 2.0f - 1.0f;
                float cy = (static_cast<float>(y) / hf) * 2.0f - 1.0f;
                float dist = std::sqrt(cx * cx + cy * cy);
                float falloff = std::pow(dist, 1.0f + rFalloff * 4.0f);

                float rOffX = static_cast<float>(rX) * falloff;
                float rOffY = static_cast<float>(rY) * falloff;
                float gOffX = static_cast<float>(gX) * falloff;
                float gOffY = static_cast<float>(gY) * falloff;
                float bOffX = static_cast<float>(bX) * falloff;
                float bOffY = static_cast<float>(bY) * falloff;

                float rRotX = rOffX * cosA - rOffY * sinA;
                float rRotY = rOffX * sinA + rOffY * cosA;
                float gRotX = gOffX * cosA - gOffY * sinA;
                float gRotY = gOffX * sinA + gOffY * cosA;
                float bRotX = bOffX * cosA - bOffY * sinA;
                float bRotY = bOffX * sinA + bOffY * cosA;

                float origR = src[y * srcStride + x * nc + 0];
                float origG = src[y * srcStride + x * nc + 1];
                float origB = src[y * srcStride + x * nc + 2];
                float origA = src[y * srcStride + x * nc + 3];

                float shiftR = bilerp(src, srcStride, srcW, srcH,
                                      static_cast<float>(x) + rRotX,
                                      static_cast<float>(y) + rRotY, 0, nc);
                float shiftG = bilerp(src, srcStride, srcW, srcH,
                                      static_cast<float>(x) + gRotX,
                                      static_cast<float>(y) + gRotY, 1, nc);
                float shiftB = bilerp(src, srcStride, srcW, srcH,
                                      static_cast<float>(x) + bRotX,
                                      static_cast<float>(y) + bRotY, 2, nc);

                dst[y * dstStride + x * nc + 0] = origR + mixF * (shiftR - origR);
                dst[y * dstStride + x * nc + 1] = origG + mixF * (shiftG - origG);
                dst[y * dstStride + x * nc + 2] = origB + mixF * (shiftB - origB);
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

        auto getDouble2D = [&](const char *name, double &x, double &y) -> bool {
            OfxParamSetHandle p;
            if (param->paramGetHandle(paramSet, name, &p, nullptr) != kOfxStatOK) return false;
            return param->paramGetValue(p, 0, &x, 1, &y) == kOfxStatOK;
        };

        double rX = 0.0, rY = 0.0, gX = 0.0, gY = 0.0, bX = 0.0, bY = 0.0;
        getDouble2D(kParamRedShift, rX, rY);
        getDouble2D(kParamGreenShift, gX, gY);
        getDouble2D(kParamBlueShift, bX, bY);

        if (isIdentityCheck(rX, rY, gX, gY, bX, bY)) {
            prop->propSetString(outArgs, kOfxImageEffectPropIsIdentityClip, 0, kClipSource);
            return kOfxStatOK;
        }
        return kOfxStatReplyDefault;
    }
};

static ChromaticAberrationPlugin s_chromabPlugin;
static PluginRegistrar s_registrar(&s_chromabPlugin);
