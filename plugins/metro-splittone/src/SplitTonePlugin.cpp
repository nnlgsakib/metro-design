// Copyright (c) 2026 Metro Design. All rights reserved.
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

static const char *kParamHighlightsHue        = "highlightsHue";
static const char *kParamHighlightsSaturation = "highlightsSaturation";
static const char *kParamShadowsHue           = "shadowsHue";
static const char *kParamShadowsSaturation    = "shadowsSaturation";
static const char *kParamBalance              = "balance";
static const char *kParamMix                  = "mix";

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

    if (mx == mn) {
        h = 0.0f;
        s = 0.0f;
        return;
    }

    float d = mx - mn;
    s = (l > 0.5f) ? d / (2.0f - mx - mn) : d / (mx + mn);

    if (mx == r) {
        h = (g - b) / d + (g < b ? 6.0f : 0.0f);
    } else if (mx == g) {
        h = (b - r) / d + 2.0f;
    } else {
        h = (r - g) / d + 4.0f;
    }
    h /= 6.0f;
}

static void hslToRgb(float h, float s, float l, float &r, float &g, float &b)
{
    if (s == 0.0f) {
        r = g = b = l;
        return;
    }

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

static float rec709Luma(const float *pixel)
{
    return 0.2126f * pixel[0] + 0.7152f * pixel[1] + 0.0722f * pixel[2];
}

class SplitTonePlugin : public Plugin {
public:
    const char *identifier() const override { return "com.metrodesign.splittone"; }
    const char *label() const override { return "Metro Split Tone"; }
    const char *description() const override {
        return "Split toning for highlights and shadows with independent hue and saturation control.";
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
        prop->propSetString(props, kOfxImageEffectPropShortLabel, 0, "MetroSplitTone");
        prop->propSetString(props, kOfxImageEffectPropLongLabel, 0, "Metro Design Split Tone");
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

        stat = defParam(kOfxParamTypeDouble, kParamHighlightsHue, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Highlights Hue");
        setHint(paramProps, "Hue applied to highlights (0-360 degrees)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 360.0);
        setDoubleDefault(paramProps, 40.0);
        setIncrement(paramProps, 1.0);
        setDigits(paramProps, 1);

        stat = defParam(kOfxParamTypeDouble, kParamHighlightsSaturation, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Highlights Saturation");
        setHint(paramProps, "Strength of the highlight tint (0=none, 1=full)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 0.0);
        setIncrement(paramProps, 0.05);
        setDigits(paramProps, 3);

        stat = defParam(kOfxParamTypeDouble, kParamShadowsHue, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Shadows Hue");
        setHint(paramProps, "Hue applied to shadows (0-360 degrees)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 360.0);
        setDoubleDefault(paramProps, 220.0);
        setIncrement(paramProps, 1.0);
        setDigits(paramProps, 1);

        stat = defParam(kOfxParamTypeDouble, kParamShadowsSaturation, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Shadows Saturation");
        setHint(paramProps, "Strength of the shadow tint (0=none, 1=full)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 0.0);
        setIncrement(paramProps, 0.05);
        setDigits(paramProps, 3);

        stat = defParam(kOfxParamTypeDouble, kParamBalance, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Balance");
        setHint(paramProps, "Shifts the split point (-1=more shadows tinted, +1=more highlights tinted)");
        setDoubleMin(paramProps, -1.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 0.0);
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

        int dstRowBytes = 0;
        prop->propGetInt(dstImgProps, kOfxImagePropRowBytes, 0, &dstRowBytes);

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

        double highlightsHue = 40.0;
        double highlightsSaturation = 0.0;
        double shadowsHue = 220.0;
        double shadowsSaturation = 0.0;
        double balance = 0.0;
        double mix = 1.0;

        if (getDouble(kParamHighlightsHue, highlightsHue) != kOfxStatOK) highlightsHue = 40.0;
        if (getDouble(kParamHighlightsSaturation, highlightsSaturation) != kOfxStatOK) highlightsSaturation = 0.0;
        if (getDouble(kParamShadowsHue, shadowsHue) != kOfxStatOK) shadowsHue = 220.0;
        if (getDouble(kParamShadowsSaturation, shadowsSaturation) != kOfxStatOK) shadowsSaturation = 0.0;
        if (getDouble(kParamBalance, balance) != kOfxStatOK) balance = 0.0;
        if (getDouble(kParamMix, mix) != kOfxStatOK) mix = 1.0;

        float hlHueF   = static_cast<float>(highlightsHue) / 360.0f;
        float hlSatF   = static_cast<float>(highlightsSaturation);
        float shHueF   = static_cast<float>(shadowsHue) / 360.0f;
        float shSatF   = static_cast<float>(shadowsSaturation);
        float balF     = static_cast<float>(balance);
        float mixF     = static_cast<float>(mix);

        const float *src = static_cast<const float *>(srcPtr);
        float *dst = static_cast<float *>(dstPtr);
        int srcStride = srcRowBytes / static_cast<int>(sizeof(float));
        int dstStride = dstRowBytes / static_cast<int>(sizeof(float));
        const int nc = 4;

        for (int y = renderY1; y < renderY2; ++y) {
            int siBase = y * srcStride;
            int diBase = y * dstStride;
            for (int x = renderX1; x < renderX2; ++x) {
                int si = siBase + x * nc;
                int di = diBase + x * nc;
                float r = src[si + 0];
                float g = src[si + 1];
                float b = src[si + 2];
                float a = src[si + 3];

                float luma = rec709Luma(src + si);
                float w = computeBlendWeight(luma, balF);

                float h, s, l;
                rgbToHsl(r, g, b, h, s, l);

                float hlDelta = shortestHueDelta(h, hlHueF);
                float shDelta = shortestHueDelta(h, shHueF);
                h += hlDelta * hlSatF * w + shDelta * shSatF * (1.0f - w);

                float resultR, resultG, resultB;
                hslToRgb(h, s, l, resultR, resultG, resultB);

                dst[di + 0] = r + mixF * (resultR - r);
                dst[di + 1] = g + mixF * (resultG - g);
                dst[di + 2] = b + mixF * (resultB - b);
                dst[di + 3] = a;
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

        double hlSat = 0.0, shSat = 0.0, mixVal = 0.0;
        getDouble(kParamHighlightsSaturation, hlSat);
        getDouble(kParamShadowsSaturation, shSat);
        getDouble(kParamMix, mixVal);

        if (hlSat == 0.0 && shSat == 0.0) {
            prop->propSetString(outArgs, kOfxImageEffectPropIsIdentityClip, 0, kClipSource);
            return kOfxStatOK;
        }
        return kOfxStatReplyDefault;
    }
};

static SplitTonePlugin s_splitTonePlugin;
static PluginRegistrar s_registrar(&s_splitTonePlugin);
