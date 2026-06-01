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

static const char *kClipSource  = "Source";
static const char *kClipSource2 = "Source2";
static const char *kClipOutput  = "Output";

static const char *kParamTransitionType = "transitionType";
static const char *kParamProgress       = "progress";
static const char *kParamEdgeSoftness   = "edgeSoftness";
static const char *kParamEdgeColor      = "edgeColor";
static const char *kParamMix            = "mix";

enum TransitionType {
    kTransitionDissolve    = 0,
    kTransitionWipeLeft    = 1,
    kTransitionWipeRight   = 2,
    kTransitionWipeUp      = 3,
    kTransitionWipeDown    = 4,
    kTransitionRadialWipe  = 5,
    kTransitionCrossZoom   = 6,
    kTransitionCircleOpen  = 7,
    kTransitionCircleClose = 8,
    kTransitionCount
};

static const char *kTransitionLabels[] = {
    "Dissolve",
    "Wipe Left",
    "Wipe Right",
    "Wipe Up",
    "Wipe Down",
    "Radial Wipe",
    "Cross Zoom",
    "Circle Open",
    "Circle Close"
};

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

static void samplePixel(const float *img, int stride, int w, int h, float x, float y, int nc, float out[4])
{
    for (int c = 0; c < nc; ++c)
        out[c] = bilerp(img, stride, w, h, x, y, c, nc);
}

class TransitionsPlugin : public Plugin {
public:
    const char *identifier() const override { return "com.metrodesign.transitions"; }
    const char *label() const override { return "Metro Transitions"; }
    const char *description() const override {
        return "Stylized video transitions: dissolves, wipes, radial wipes, cross zoom, and circle transitions.";
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
        prop->propSetString(props, kOfxImageEffectPropShortLabel, 0, "MetroTrans");
        prop->propSetString(props, kOfxImageEffectPropLongLabel, 0, "Metro Design Transitions");
        prop->propSetString(props, kOfxImageEffectPropGrouping, 0, pluginGrouping());
        prop->propSetString(props, kOfxImageEffectPropDescription, 0, description());

        const char *contexts[] = { kOfxImageEffectContextTransition, nullptr };
        prop->propSetStringN(props, kOfxImageEffectPropSupportedContexts, 1, contexts);

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

        effect->clipDefine(descriptor, kClipSource2, &clipProps);
        if (prop) {
            prop->propSetString(clipProps, kOfxImageEffectPropLabel, 0, "Source2");
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

        stat = defParam(kOfxParamTypeChoice, kParamTransitionType, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Transition Type");
        setHint(paramProps, "Select the transition style");
        prop->propSetStringN(paramProps, kOfxParamPropChoiceOption,
                             kTransitionCount, kTransitionLabels);
        prop->propSetStringN(paramProps, kOfxParamPropChoiceLabelOption,
                             kTransitionCount, kTransitionLabels);
        prop->propSetInt(paramProps, kOfxParamPropIntDefault, 0, 0);

        stat = defParam(kOfxParamTypeDouble, kParamProgress, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Progress");
        setHint(paramProps, "Transition progress (0=full A, 1=full B)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 0.0);
        setIncrement(paramProps, 0.01);
        setDigits(paramProps, 3);

        stat = defParam(kOfxParamTypeDouble, kParamEdgeSoftness, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Edge Softness");
        setHint(paramProps, "Softness of the transition edge (0=hard, 1=very soft)");
        setDoubleMin(paramProps, 0.0);
        setDoubleMax(paramProps, 1.0);
        setDoubleDefault(paramProps, 0.1);
        setIncrement(paramProps, 0.05);
        setDigits(paramProps, 3);

        stat = defParam(kOfxParamTypeRGB, kParamEdgeColor, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Edge Color");
        setHint(paramProps, "Color of the transition edge line");
        prop->propSetDouble(paramProps, kOfxParamPropDoubleDefault, 0, 0.0);
        prop->propSetDouble(paramProps, kOfxParamPropDoubleDefault, 1, 0.0);
        prop->propSetDouble(paramProps, kOfxParamPropDoubleDefault, 2, 0.0);

        stat = defParam(kOfxParamTypeDouble, kParamMix, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Mix");
        setHint(paramProps, "Blend between original and transition (0=no transition, 1=full transition)");
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
        OfxImageEffectHandle src2Clip = nullptr;
        OfxPropertySetHandle src2ClipProps = nullptr;
        OfxImageEffectHandle dstClip = nullptr;
        OfxPropertySetHandle dstClipProps = nullptr;

        OfxStatus stat = effect->clipGetHandle(instance, kClipSource, &srcClip, &srcClipProps);
        if (stat != kOfxStatOK || !srcClip) return kOfxStatErrBadHandle;

        stat = effect->clipGetHandle(instance, kClipSource2, &src2Clip, &src2ClipProps);
        if (stat != kOfxStatOK || !src2Clip) return kOfxStatErrBadHandle;

        stat = effect->clipGetHandle(instance, kClipOutput, &dstClip, &dstClipProps);
        if (stat != kOfxStatOK || !dstClip) return kOfxStatErrBadHandle;

        OfxPropertySetHandle srcImgProps = nullptr;
        OfxPropertySetHandle srcData = nullptr;
        OfxPropertySetHandle src2ImgProps = nullptr;
        OfxPropertySetHandle src2Data = nullptr;
        OfxPropertySetHandle dstImgProps = nullptr;
        OfxPropertySetHandle dstData = nullptr;

        stat = effect->imageClipGetImage(srcClip, time, nullptr, &srcImgProps, &srcData);
        if (stat != kOfxStatOK || !srcImgProps || !srcData) return kOfxStatErrBadHandle;

        stat = effect->imageClipGetImage(src2Clip, time, nullptr, &src2ImgProps, &src2Data);
        if (stat != kOfxStatOK || !src2ImgProps || !src2Data) {
            effect->imageClipReleaseImage(srcData);
            return kOfxStatErrBadHandle;
        }

        stat = effect->imageClipGetImage(dstClip, time, nullptr, &dstImgProps, &dstData);
        if (stat != kOfxStatOK || !dstImgProps || !dstData) {
            effect->imageClipReleaseImage(srcData);
            effect->imageClipReleaseImage(src2Data);
            return kOfxStatErrBadHandle;
        }

        int srcRowBytes = 0;
        prop->propGetInt(srcImgProps, kOfxImagePropRowBytes, 0, &srcRowBytes);
        int srcBounds[4] = {0, 0, 0, 0};
        prop->propGetIntN(srcImgProps, kOfxImageEffectPropBounds, 4, srcBounds);
        int srcW = srcBounds[2] - srcBounds[0];
        int srcH = srcBounds[3] - srcBounds[1];

        int src2RowBytes = 0;
        prop->propGetInt(src2ImgProps, kOfxImagePropRowBytes, 0, &src2RowBytes);
        int src2Bounds[4] = {0, 0, 0, 0};
        prop->propGetIntN(src2ImgProps, kOfxImageEffectPropBounds, 4, src2Bounds);
        int src2W = src2Bounds[2] - src2Bounds[0];
        int src2H = src2Bounds[3] - src2Bounds[1];

        int dstRowBytes = 0;
        prop->propGetInt(dstImgProps, kOfxImagePropRowBytes, 0, &dstRowBytes);
        int dstBounds[4] = {0, 0, 0, 0};
        prop->propGetIntN(dstImgProps, kOfxImageEffectPropBounds, 4, dstBounds);
        int dstW = dstBounds[2] - dstBounds[0];
        int dstH = dstBounds[3] - dstBounds[1];

        void *srcPtr = nullptr;
        void *src2Ptr = nullptr;
        void *dstPtr = nullptr;
        prop->propGetPointer(srcImgProps, kOfxImageEffectPropData, 0, &srcPtr);
        prop->propGetPointer(src2ImgProps, kOfxImageEffectPropData, 0, &src2Ptr);
        prop->propGetPointer(dstImgProps, kOfxImageEffectPropData, 0, &dstPtr);
        if (!srcPtr || !src2Ptr || !dstPtr) {
            effect->imageClipReleaseImage(srcData);
            effect->imageClipReleaseImage(src2Data);
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
            effect->imageClipReleaseImage(src2Data);
            effect->imageClipReleaseImage(dstData);
            return stat;
        }

        auto getInt = [&](const char *name, int &v) -> OfxStatus {
            OfxParamSetHandle p;
            OfxStatus s = param->paramGetHandle(paramSet, name, &p, nullptr);
            if (s != kOfxStatOK) return s;
            return param->paramGetValue(p, 0, &v);
        };
        auto getDouble = [&](const char *name, double &v) -> OfxStatus {
            OfxParamSetHandle p;
            OfxStatus s = param->paramGetHandle(paramSet, name, &p, nullptr);
            if (s != kOfxStatOK) return s;
            return param->paramGetValue(p, 0, &v);
        };
        auto getDouble3 = [&](const char *name, double &r, double &g, double &b) -> OfxStatus {
            OfxParamSetHandle p;
            OfxStatus s = param->paramGetHandle(paramSet, name, &p, nullptr);
            if (s != kOfxStatOK) return s;
            return param->paramGetValue(p, 0, &r, 1, &g, 2, &b);
        };

        int transitionType = 0;
        double progress = 0.0, edgeSoftness = 0.1, mix = 1.0;
        double edgeR = 0.0, edgeG = 0.0, edgeB = 0.0;

        if (getInt(kParamTransitionType, transitionType) != kOfxStatOK) transitionType = 0;
        if (getDouble(kParamProgress, progress) != kOfxStatOK) progress = 0.0;
        if (getDouble(kParamEdgeSoftness, edgeSoftness) != kOfxStatOK) edgeSoftness = 0.1;
        if (getDouble3(kParamEdgeColor, edgeR, edgeG, edgeB) != kOfxStatOK) { edgeR = 0.0; edgeG = 0.0; edgeB = 0.0; }
        if (getDouble(kParamMix, mix) != kOfxStatOK) mix = 1.0;

        const float *src = static_cast<const float *>(srcPtr);
        const float *src2 = static_cast<const float *>(src2Ptr);
        float *dst = static_cast<float *>(dstPtr);
        int srcStride = srcRowBytes / static_cast<int>(sizeof(float));
        int src2Stride = src2RowBytes / static_cast<int>(sizeof(float));
        int dstStride = dstRowBytes / static_cast<int>(sizeof(float));
        const int nc = 4;
        float p = clampf(static_cast<float>(progress), 0.0f, 1.0f);
        float softness = clampf(static_cast<float>(edgeSoftness), 0.0f, 1.0f);
        float mixF = static_cast<float>(mix);
        float eR = static_cast<float>(edgeR);
        float eG = static_cast<float>(edgeG);
        float eB = static_cast<float>(edgeB);

        for (int y = renderY1; y < renderY2; ++y) {
            for (int x = renderX1; x < renderX2; ++x) {
                float fx = static_cast<float>(x);
                float fy = static_cast<float>(y);

                float srcA[4], srcB[4];
                samplePixel(src, srcStride, srcW, srcH, fx, fy, nc, srcA);
                samplePixel(src2, src2Stride, src2W, src2H, fx, fy, nc, srcB);

                float blend, edgeWeight;
                computeTransition(transitionType, p, fx, fy,
                                  static_cast<float>(dstW),
                                  static_cast<float>(dstH),
                                  softness, blend, edgeWeight);

                float out[4];
                for (int c = 0; c < 3; ++c) {
                    float a = srcA[c];
                    float b = srcB[c];
                    float transVal = a + blend * (b - a);
                    float eCol = (c == 0) ? eR : ((c == 1) ? eG : eB);
                    float edgeVal = transVal + edgeWeight * (eCol - transVal);
                    out[c] = srcA[c] + mixF * (edgeVal - srcA[c]);
                }
                out[3] = srcA[3];

                dst[y * dstStride + x * nc + 0] = out[0];
                dst[y * dstStride + x * nc + 1] = out[1];
                dst[y * dstStride + x * nc + 2] = out[2];
                dst[y * dstStride + x * nc + 3] = out[3];
            }
        }

        effect->imageClipReleaseImage(srcData);
        effect->imageClipReleaseImage(src2Data);
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

        double mix = 1.0;
        getDouble(kParamMix, mix);
        if (mix <= 0.0) {
            prop->propSetString(outArgs, kOfxImageEffectPropIsIdentityClip, 0, kClipSource);
            return kOfxStatOK;
        }

        double progress = 0.0;
        getDouble(kParamProgress, progress);

        if (progress <= 0.0) {
            prop->propSetString(outArgs, kOfxImageEffectPropIsIdentityClip, 0, kClipSource);
            return kOfxStatOK;
        }
        if (progress >= 1.0) {
            prop->propSetString(outArgs, kOfxImageEffectPropIsIdentityClip, 0, kClipSource2);
            return kOfxStatOK;
        }

        return kOfxStatReplyDefault;
    }

private:
    void computeTransition(int type, float p, float x, float y,
                          float w, float h, float softness,
                          float &blend, float &edgeWeight) const
    {
        switch (type) {
            case kTransitionDissolve:
                dissolve(p, softness, blend, edgeWeight);
                break;
            case kTransitionWipeLeft:
                wipe(x / w, p, softness, blend, edgeWeight);
                break;
            case kTransitionWipeRight:
                wipe(1.0f - x / w, p, softness, blend, edgeWeight);
                break;
            case kTransitionWipeUp:
                wipe(y / h, p, softness, blend, edgeWeight);
                break;
            case kTransitionWipeDown:
                wipe(1.0f - y / h, p, softness, blend, edgeWeight);
                break;
            case kTransitionRadialWipe:
                radialWipe(x, y, w, h, p, softness, blend, edgeWeight);
                break;
            case kTransitionCrossZoom:
                crossZoom(p, softness, blend, edgeWeight);
                break;
            case kTransitionCircleOpen:
                circleWipe(x, y, w, h, p, softness, true, blend, edgeWeight);
                break;
            case kTransitionCircleClose:
                circleWipe(x, y, w, h, 1.0f - p, softness, true, blend, edgeWeight);
                break;
            default:
                dissolve(p, softness, blend, edgeWeight);
                break;
        }
    }

    static void dissolve(float p, float softness, float &blend, float &edgeWeight)
    {
        (void)softness;
        blend = p;
        edgeWeight = 0.0f;
    }

    static void wipe(float pos, float p, float softness, float &blend, float &edgeWeight)
    {
        float edge = p;
        float d = pos - edge;
        if (softness > 0.001f) {
            float halfSoft = std::max(softness * 0.5f, 0.001f);
            float t = (d + halfSoft) / (2.0f * halfSoft);
            t = clampf(t, 0.0f, 1.0f);
            blend = t * t * (3.0f - 2.0f * t);
            float edgeDist = std::abs(d) / halfSoft;
            edgeWeight = 1.0f - clampf(edgeDist, 0.0f, 1.0f);
            edgeWeight = edgeWeight * edgeWeight * (3.0f - 2.0f * edgeWeight);
        } else {
            blend = (d >= 0.0f) ? 1.0f : 0.0f;
            edgeWeight = 0.0f;
        }
    }

    static void radialWipe(float x, float y, float w, float h, float p, float softness,
                           float &blend, float &edgeWeight)
    {
        float cx = x - w * 0.5f;
        float cy = y - h * 0.5f;
        float angle = std::atan2(cy, cx);
        float angleNorm = angle / (2.0f * 3.14159265358979323846f);
        if (angleNorm < 0.0f) angleNorm += 1.0f;

        float sweep = p;
        float d = angleNorm - sweep;
        if (d < 0.0f) d += 1.0f;
        d = d - 0.5f;

        if (softness > 0.001f) {
            float halfSoft = std::max(softness * 0.5f, 0.001f);
            float t = 1.0f - (d + halfSoft) / (2.0f * halfSoft);
            t = clampf(t, 0.0f, 1.0f);
            blend = t * t * (3.0f - 2.0f * t);
            float edgeDist = std::abs(d) / halfSoft;
            edgeWeight = 1.0f - clampf(edgeDist, 0.0f, 1.0f);
            edgeWeight = edgeWeight * edgeWeight * (3.0f - 2.0f * edgeWeight);
        } else {
            blend = (d >= 0.0f) ? 0.0f : 1.0f;
            edgeWeight = 0.0f;
        }
    }

    static void circleWipe(float x, float y, float w, float h, float p, float softness,
                           bool insideShowsB, float &blend, float &edgeWeight)
    {
        float cx = w * 0.5f;
        float cy = h * 0.5f;
        float maxDim = std::sqrt(cx * cx + cy * cy);
        float dist = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) / maxDim;
        float radius = p;

        float d = dist - radius;
        if (softness > 0.001f) {
            float halfSoft = std::max(softness * 0.5f, 0.001f);
            float t = (d + halfSoft) / (2.0f * halfSoft);
            t = clampf(t, 0.0f, 1.0f);
            if (insideShowsB) t = 1.0f - t;
            blend = t * t * (3.0f - 2.0f * t);
            float edgeDist = std::abs(d) / halfSoft;
            edgeWeight = 1.0f - clampf(edgeDist, 0.0f, 1.0f);
            edgeWeight = edgeWeight * edgeWeight * (3.0f - 2.0f * edgeWeight);
        } else {
            if (insideShowsB) {
                blend = (d <= 0.0f) ? 1.0f : 0.0f;
            } else {
                blend = (d <= 0.0f) ? 0.0f : 1.0f;
            }
            edgeWeight = 0.0f;
        }
    }

    static void crossZoom(float p, float softness, float &blend, float &edgeWeight)
    {
        (void)softness;
        blend = p;
        edgeWeight = 0.0f;
    }
};

static TransitionsPlugin s_transitionsPlugin;
static PluginRegistrar s_registrar(&s_transitionsPlugin);
