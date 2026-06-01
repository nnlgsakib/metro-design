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

// ---------------------------------------------------------------------------
// Color space & gamut mapping enums
// ---------------------------------------------------------------------------

enum ColorSpace {
    kCS_sRGB    = 0,
    kCS_Rec709  = 1,
    kCS_Rec2020 = 2,
    kCS_ACEScct = 3,
    kCS_LogC    = 4,
    kCS_SLog3   = 5,
    kCS_SLog2   = 6,
    kCS_VLog    = 7,
    kCS_Count
};

static const char *kColorSpaceNames[kCS_Count] = {
    "sRGB", "Rec.709", "Rec.2020", "ACEScct", "LogC", "S-Log3", "S-Log2", "V-Log"
};

enum GamutMapMode {
    kGamut_None    = 0,
    kGamut_Clamp   = 1,
    kGamut_SoftClip = 2
};

static const char *kGamutNames[3] = { "None", "Clamp", "Soft Clip" };

// ---------------------------------------------------------------------------
// OFX property / clip / param string constants
// ---------------------------------------------------------------------------

static const char *kClipSource = "Source";
static const char *kClipOutput = "Output";

static const char *kParamInputCS   = "inputCS";
static const char *kParamOutputCS  = "outputCS";
static const char *kParamGamut     = "gamutMap";
static const char *kParamMix       = "mix";

// ---------------------------------------------------------------------------
// Math helpers
// ---------------------------------------------------------------------------

static float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

// ---------------------------------------------------------------------------
// Transfer functions — inverse OETF (encoded -> linear) and forward OETF
// ---------------------------------------------------------------------------

// --- sRGB ---
static float srgb_inv(float v) {
    if (v <= 0.04045f) return v / 12.92f;
    return powf((v + 0.055f) / 1.055f, 2.4f);
}
static float srgb_fwd(float v) {
    if (v <= 0.0031308f) return v * 12.92f;
    return 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
}

// --- Rec.709 / BT.709 ---
static float rec709_inv(float v) {
    if (v < 0.081f) return v / 4.5f;
    return powf((v + 0.099f) / 1.099f, 1.0f / 0.45f);
}
static float rec709_fwd(float v) {
    if (v < 0.018f) return v * 4.5f;
    return 1.099f * powf(v, 0.45f) - 0.099f;
}

// --- Rec.2020 (BT.2020 gamma) ---
// BT.2020 OETF is identical to BT.709
static float rec2020_inv(float v) { return rec709_inv(v); }
static float rec2020_fwd(float v) { return rec709_fwd(v); }

// --- ACEScct ---
// Reference: SMPTE ST 2065-1 / ACES 1.3
static float acescct_inv(float v) {
    const float kLinSlope = 10.5402377416545f;
    const float kLinOff   = 0.0729055341958355f;
    const float kLogCut   = (log2f(0.0078125f) + 9.72f) / 17.52f;
    const float kBelowCut = kLinOff;
    if (v >= kLogCut) return powf(2.0f, v * 17.52f - 9.72f);
    if (v >= kBelowCut) return (v - kLinOff) / kLinSlope;
    return (v - 0.5f) * 2.0f;
}
static float acescct_fwd(float v) {
    const float kLinSlope = 10.5402377416545f;
    const float kLinOff   = 0.0729055341958355f;
    const float kLogCut   = 0.0078125f;
    if (v < 0.0f) return v * 0.5f + 0.5f;
    if (v < kLogCut) return kLinSlope * v + kLinOff;
    return (log2f(v) + 9.72f) / 17.52f;
}

// --- ARRI LogC v3 (EI 800) ---
// Reference: ARRI LogC3 Algorithm
static float logc_inv(float v) {
    const float a = 5.555556f;
    const float b = 0.052272f;
    const float c = 0.012852f;
    const float cut = 0.010591f;
    const float logCut = c + a * log10f(cut + b);
    if (v >= logCut) return powf(10.0f, (v - c) / a) - b;
    return (cut / logCut) * v;
}
static float logc_fwd(float v) {
    const float a = 5.555556f;
    const float b = 0.052272f;
    const float c = 0.012852f;
    const float cut = 0.010591f;
    const float logCut = c + a * log10f(cut + b);
    if (v >= cut) return c + a * log10f(v + b);
    return (logCut / cut) * v;
}

// --- S-Log3 ---
// Reference: Sony S-Log3 specification
static float slog3_inv(float v) {
    const float a = 0.432699f;
    const float b = 0.037584f;
    const float c = 0.016119f;
    const float cut = 0.011599f;
    if (v >= cut) return powf(10.0f, (v - c) / a) - b;
    return (v / cut) * (powf(10.0f, (cut - c) / a) - b);
}
static float slog3_fwd(float v) {
    const float a = 0.432699f;
    const float b = 0.037584f;
    const float c = 0.016119f;
    if (v >= 0.0f) return c + a * log10f(v + b);
    return c + a * log10f(b);
}

// --- S-Log2 ---
// Reference: Sony S-Log2 specification
static float slog2_inv(float v) {
    const float a = 0.255649f;
    const float b = 0.003373f;
    const float c = 0.030601f;
    const float cut = 0.030967f;
    if (v >= cut) return (powf(10.0f, (v - c) / a) - b) / 0.555556f;
    return (v / cut) * 0.0001f;
}
static float slog2_fwd(float v) {
    const float a = 0.255649f;
    const float b = 0.003373f;
    const float c = 0.030601f;
    if (v >= 0.0f) return c + a * log10f(v * 0.555556f + b);
    return c + a * log10f(b);
}

// --- V-Log ---
// Reference: Panasonic V-Log / V-Gamut specification
static float vlog_inv(float v) {
    const float a = 0.125f;
    const float b = 0.0075f;
    const float c = 0.5f;
    const float d = 0.1f;
    const float e = 0.0095f;
    const float cut = 0.100962f;
    if (v >= cut) return powf(10.0f, (v - d) / a) - b;
    return (v - c) / (e * 16.0f);
}
static float vlog_fwd(float v) {
    const float a = 0.125f;
    const float b = 0.0075f;
    const float c = 0.5f;
    const float d = 0.1f;
    const float e = 0.0095f;
    const float cut = 0.100962f;
    if (v >= cut) return d + a * log10f(v + b);
    return c + e * 16.0f * v;
}

// ---------------------------------------------------------------------------
// Transfer function dispatch tables
// ---------------------------------------------------------------------------

using TransferFn = float (*)(float);

static TransferFn kInvOETF[kCS_Count] = {
    srgb_inv,     // sRGB
    rec709_inv,   // Rec.709
    rec2020_inv,  // Rec.2020
    acescct_inv,  // ACEScct
    logc_inv,     // LogC
    slog3_inv,    // S-Log3
    slog2_inv,    // S-Log2
    vlog_inv      // V-Log
};

static TransferFn kOETF[kCS_Count] = {
    srgb_fwd,     // sRGB
    rec709_fwd,   // Rec.709
    rec2020_fwd,  // Rec.2020
    acescct_fwd,  // ACEScct
    logc_fwd,     // LogC
    slog3_fwd,    // S-Log3
    slog2_fwd,    // S-Log2
    vlog_fwd      // V-Log
};

// ---------------------------------------------------------------------------
// Color primary matrix data
//
// All matrices convert linear RGB <-> CIE XYZ (D65).
// For ACES AP1 (D60 white) the matrix includes Bradford chromatic adaptation
// D60->D65 so all spaces use a common XYZ D65 interchange.
// ---------------------------------------------------------------------------

struct ColorMatrix {
    float m[3][3];
};

// sRGB / Rec.709 -> XYZ D65
static const ColorMatrix kMat709_to_XYZ = {{
    { 0.412391f,  0.357584f,  0.180481f },
    { 0.212639f,  0.715169f,  0.072192f },
    { 0.019331f,  0.119195f,  0.950532f }
}};

// XYZ D65 -> sRGB / Rec.709
static const ColorMatrix kXYZ_to_709 = {{
    {  3.240970f, -1.537383f, -0.498611f },
    { -0.969244f,  1.875968f,  0.041555f },
    {  0.055630f, -0.203977f,  1.056972f }
}};

// Rec.2020 -> XYZ D65
static const ColorMatrix kMat2020_to_XYZ = {{
    { 0.636958f,  0.144617f,  0.168881f },
    { 0.262700f,  0.677998f,  0.059302f },
    { 0.000000f,  0.028073f,  1.060985f }
}};

// XYZ D65 -> Rec.2020
static const ColorMatrix kXYZ_to_2020 = {{
    {  1.716651f, -0.355671f, -0.253366f },
    { -0.666684f,  1.616481f,  0.015769f },
    {  0.017640f, -0.042771f,  0.942103f }
}};

// ACES AP1 (D60->D65 Bradford adaptation built in) -> XYZ D65
static const ColorMatrix kMatAP1d65_to_XYZ = {{
    { 0.661285f,  0.133693f,  0.155583f },
    { 0.271003f,  0.672188f,  0.053446f },
    { 0.002044f,  0.006852f,  1.018549f }
}};

// XYZ D65 -> ACES AP1 (D65->D60 Bradford adaptation built in)
static const ColorMatrix kXYZ_to_AP1d65 = {{
    {  1.645470f, -0.326091f, -0.239299f },
    { -0.662750f,  1.575103f,  0.044385f },
    {  0.004015f, -0.011344f,  0.980924f }
}};

// ARRI Wide Gamut -> XYZ D65
static const ColorMatrix kMatAWG_to_XYZ = {{
    { 0.666091f,  0.150909f,  0.132820f },
    { 0.241211f,  0.717425f,  0.041364f },
    { 0.009112f,  0.096083f,  0.890041f }
}};

// XYZ D65 -> ARRI Wide Gamut
static const ColorMatrix kXYZ_to_AWG = {{
    {  1.602026f, -0.338946f, -0.149606f },
    { -0.537626f,  1.495562f,  0.026714f },
    {  0.035296f, -0.147970f,  1.131602f }
}};

// Sony S-Gamut3 -> XYZ D65
static const ColorMatrix kMatSG3_to_XYZ = {{
    { 0.652221f,  0.153347f,  0.139564f },
    { 0.230452f,  0.729201f,  0.040347f },
    { 0.010620f,  0.037713f,  0.942189f }
}};

// XYZ D65 -> Sony S-Gamut3
static const ColorMatrix kXYZ_to_SG3 = {{
    {  1.599125f, -0.336898f, -0.192469f },
    { -0.504621f,  1.433293f,  0.048759f },
    { -0.008094f, -0.052606f,  1.068210f }
}};

// Sony S-Gamut -> XYZ D65
static const ColorMatrix kMatSG_to_XYZ = {{
    { 0.644466f,  0.158591f,  0.139182f },
    { 0.228590f,  0.729624f,  0.041787f },
    { 0.010657f,  0.030722f,  0.957781f }
}};

// XYZ D65 -> Sony S-Gamut
static const ColorMatrix kXYZ_to_SG = {{
    {  1.618408f, -0.352926f, -0.192803f },
    { -0.506826f,  1.435195f,  0.050231f },
    { -0.005178f, -0.043853f,  1.051719f }
}};

// Panasonic V-Gamut -> XYZ D65
static const ColorMatrix kMatVG_to_XYZ = {{
    { 0.618340f,  0.153325f,  0.174800f },
    { 0.221093f,  0.716394f,  0.062513f },
    { 0.005235f,  0.047621f,  0.925401f }
}};

// XYZ D65 -> Panasonic V-Gamut
static const ColorMatrix kXYZ_to_VG = {{
    {  1.671541f, -0.355653f, -0.232426f },
    { -0.514642f,  1.443139f,  0.054136f },
    {  0.003578f, -0.068370f,  1.079887f }
}};

// ---------------------------------------------------------------------------
// Per-color-space matrix dispatch
// ---------------------------------------------------------------------------

static const ColorMatrix *kRGB_to_XYZ[kCS_Count] = {
    &kMat709_to_XYZ,    // sRGB
    &kMat709_to_XYZ,    // Rec.709
    &kMat2020_to_XYZ,   // Rec.2020
    &kMatAP1d65_to_XYZ, // ACEScct
    &kMatAWG_to_XYZ,    // LogC
    &kMatSG3_to_XYZ,    // S-Log3
    &kMatSG_to_XYZ,     // S-Log2
    &kMatVG_to_XYZ      // V-Log
};

static const ColorMatrix *kXYZ_to_RGB[kCS_Count] = {
    &kXYZ_to_709,    // sRGB
    &kXYZ_to_709,    // Rec.709
    &kXYZ_to_2020,   // Rec.2020
    &kXYZ_to_AP1d65, // ACEScct
    &kXYZ_to_AWG,    // LogC
    &kXYZ_to_SG3,    // S-Log3
    &kXYZ_to_SG,     // S-Log2
    &kXYZ_to_VG      // V-Log
};

// ---------------------------------------------------------------------------
// Matrix multiply:  vec3_out = M * vec3_in
// ---------------------------------------------------------------------------

static void applyMatrix(const ColorMatrix &M, const float in[3], float out[3]) {
    out[0] = M.m[0][0] * in[0] + M.m[0][1] * in[1] + M.m[0][2] * in[2];
    out[1] = M.m[1][0] * in[0] + M.m[1][1] * in[1] + M.m[1][2] * in[2];
    out[2] = M.m[2][0] * in[0] + M.m[2][1] * in[1] + M.m[2][2] * in[2];
}

// ---------------------------------------------------------------------------
// Gamut mapping
// ---------------------------------------------------------------------------

static void gamutNone(const float rgb[3], float out[3]) {
    out[0] = rgb[0]; out[1] = rgb[1]; out[2] = rgb[2];
}

static void gamutClamp(const float rgb[3], float out[3]) {
    out[0] = clampf(rgb[0], 0.0f, 1.0f);
    out[1] = clampf(rgb[1], 0.0f, 1.0f);
    out[2] = clampf(rgb[2], 0.0f, 1.0f);
}

static void gamutSoftClip(const float rgb[3], float out[3]) {
    const float knee  = 0.85f;
    const float limit = 1.15f;
    const float invRange = 1.0f / (limit - knee);
    for (int i = 0; i < 3; ++i) {
        float v = rgb[i];
        if (v <= 0.0f) { out[i] = 0.0f; }
        else if (v <= knee) { out[i] = v; }
        else if (v < limit) {
            float t = (v - knee) * invRange;
            out[i] = knee + (limit - knee) * (t * (2.0f - t));
        } else {
            out[i] = limit;
        }
    }
}

using GamutFn = void (*)(const float *, float *);

static GamutFn kGamutFns[3] = {
    gamutNone, gamutClamp, gamutSoftClip
};

// ---------------------------------------------------------------------------
// Plugin class
// ---------------------------------------------------------------------------

class ColorSpacePlugin : public Plugin {
public:
    const char *identifier() const override { return "com.metrodesign.colorspace"; }
    const char *label() const override { return "Metro Color Space"; }
    const char *description() const override {
        return "Color space conversion between sRGB, Rec.709, Rec.2020, ACEScct, "
               "LogC, S-Log3, S-Log2, and V-Log with gamut mapping.";
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
        prop->propSetString(props, kOfxImageEffectPropShortLabel, 0, "MetroColorS");
        prop->propSetString(props, kOfxImageEffectPropLongLabel, 0, "Metro Design Color Space");
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

        // --- Input Colorspace (choice) ---
        stat = defParam(kOfxParamTypeChoice, kParamInputCS, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Input Colorspace");
        setHint(paramProps, "Source color space");
        for (int i = 0; i < kCS_Count; ++i)
            prop->propSetString(paramProps, kOfxParamPropChoiceOption, i, kColorSpaceNames[i]);
        prop->propSetInt(paramProps, kOfxParamPropIntDefault, 0, 1);

        // --- Output Colorspace (choice) ---
        stat = defParam(kOfxParamTypeChoice, kParamOutputCS, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Output Colorspace");
        setHint(paramProps, "Target color space");
        for (int i = 0; i < kCS_Count; ++i)
            prop->propSetString(paramProps, kOfxParamPropChoiceOption, i, kColorSpaceNames[i]);
        prop->propSetInt(paramProps, kOfxParamPropIntDefault, 0, 0);

        // --- Gamut Mapping (choice) ---
        stat = defParam(kOfxParamTypeChoice, kParamGamut, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Gamut Mapping");
        setHint(paramProps, "How to handle out-of-gamut colors");
        for (int i = 0; i < 3; ++i)
            prop->propSetString(paramProps, kOfxParamPropChoiceOption, i, kGamutNames[i]);
        prop->propSetInt(paramProps, kOfxParamPropIntDefault, 0, 0);

        // --- Mix (double) ---
        stat = defParam(kOfxParamTypeDouble, kParamMix, paramProps);
        if (stat != kOfxStatOK) return stat;
        setLabel(paramProps, "Mix");
        setHint(paramProps, "Blend between original and converted (0=original, 1=full conversion)");
        if (prop) {
            prop->propSetDouble(paramProps, kOfxParamPropDoubleMin, 0, 0.0);
            prop->propSetDouble(paramProps, kOfxParamPropDoubleMax, 0, 1.0);
            prop->propSetDouble(paramProps, kOfxParamPropDoubleDefault, 0, 1.0);
            prop->propSetDouble(paramProps, kOfxParamPropIncrement, 0, 0.05);
            prop->propSetInt(paramProps, kOfxParamPropDigits, 0, 3);
        }

        return kOfxStatOK;
    }

    OfxStatus render(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs,
                     OfxPropertySetHandle outArgs) override
    {
        (void)outArgs;
        if (!hostAvailable() || !host().imageEffect() || !host().properties() || !host().parameters())
            return kOfxStatErrBadHandle;

        const OfxImageEffectSuiteV1 *effect = host().imageEffect();
        const OfxPropertySuiteV1 *prop = host().properties();
        const OfxParamSuiteV1 *param = host().parameters();

        double time = 0.0;
        if (prop->propGetDouble(inArgs, kOfxPropTime, 0, &time) != kOfxStatOK) time = 0.0;

        OfxImageEffectHandle srcClip = nullptr;
        OfxPropertySetHandle srcClipProps = nullptr;
        OfxImageEffectHandle dstClip = nullptr;
        OfxPropertySetHandle dstClipProps = nullptr;

        OfxStatus stat = effect->clipGetHandle(instance, kClipSource, &srcClip, &srcClipProps);
        if (stat != kOfxStatOK || !srcClip) return kOfxStatErrBadHandle;

        stat = effect->clipGetHandle(instance, kClipOutput, &dstClip, &dstClipProps);
        if (stat != kOfxStatOK || !dstClip) return kOfxStatErrBadHandle;

        OfxPropertySetHandle srcImgProps = nullptr, srcData = nullptr;
        OfxPropertySetHandle dstImgProps = nullptr, dstData = nullptr;

        stat = effect->imageClipGetImage(srcClip, time, nullptr, &srcImgProps, &srcData);
        if (stat != kOfxStatOK || !srcImgProps || !srcData) return kOfxStatErrBadHandle;

        stat = effect->imageClipGetImage(dstClip, time, nullptr, &dstImgProps, &dstData);
        if (stat != kOfxStatOK || !dstImgProps || !dstData) {
            effect->imageClipReleaseImage(srcData);
            return kOfxStatErrBadHandle;
        }

        int srcRowBytes = 0;
        prop->propGetInt(srcImgProps, kOfxImagePropRowBytes, 0, &srcRowBytes);
        int srcBounds[4] = {0,0,0,0};
        prop->propGetIntN(srcImgProps, kOfxImageEffectPropBounds, 4, srcBounds);
        int srcW = srcBounds[2] - srcBounds[0];

        int dstRowBytes = 0;
        prop->propGetInt(dstImgProps, kOfxImagePropRowBytes, 0, &dstRowBytes);
        int dstBounds[4] = {0,0,0,0};
        prop->propGetIntN(dstImgProps, kOfxImageEffectPropBounds, 4, dstBounds);
        (void)srcW;

        void *srcPtr = nullptr, *dstPtr = nullptr;
        prop->propGetPointer(srcImgProps, kOfxImageEffectPropData, 0, &srcPtr);
        prop->propGetPointer(dstImgProps, kOfxImageEffectPropData, 0, &dstPtr);
        if (!srcPtr || !dstPtr) {
            effect->imageClipReleaseImage(srcData);
            effect->imageClipReleaseImage(dstData);
            return kOfxStatErrBadHandle;
        }

        int renderWindow[4] = {0,0,0,0};
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

        auto getIntChoice = [&](const char *name, int &v) -> OfxStatus {
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

        int inputCS = kCS_Rec709;
        int outputCS = kCS_sRGB;
        int gamutMode = kGamut_None;
        double mix = 1.0;

        if (getIntChoice(kParamInputCS, inputCS) != kOfxStatOK) inputCS = kCS_Rec709;
        if (getIntChoice(kParamOutputCS, outputCS) != kOfxStatOK) outputCS = kCS_sRGB;
        if (getIntChoice(kParamGamut, gamutMode) != kOfxStatOK) gamutMode = kGamut_None;
        if (getDouble(kParamMix, mix) != kOfxStatOK) mix = 1.0;

        inputCS = std::max(0, std::min(kCS_Count - 1, inputCS));
        outputCS = std::max(0, std::min(kCS_Count - 1, outputCS));
        gamutMode = std::max(0, std::min(2, gamutMode));

        TransferFn invOETF = kInvOETF[inputCS];
        TransferFn fwdOETF = kOETF[outputCS];
        const ColorMatrix &rgb2xyz = *kRGB_to_XYZ[inputCS];
        const ColorMatrix &xyz2rgb = *kXYZ_to_RGB[outputCS];
        GamutFn gamutFn = kGamutFns[gamutMode];

        const float *src = static_cast<const float *>(srcPtr);
        float *dst = static_cast<float *>(dstPtr);
        int srcStride = srcRowBytes / static_cast<int>(sizeof(float));
        int dstStride = dstRowBytes / static_cast<int>(sizeof(float));
        const int nc = 4;
        float mixF = static_cast<float>(mix);

        for (int y = renderY1; y < renderY2; ++y) {
            for (int x = renderX1; x < renderX2; ++x) {
                int si = y * srcStride + x * nc;
                int di = y * dstStride + x * nc;

                float orig[4] = { src[si + 0], src[si + 1], src[si + 2], src[si + 3] };

                float lin[3];
                lin[0] = invOETF(orig[0]);
                lin[1] = invOETF(orig[1]);
                lin[2] = invOETF(orig[2]);

                float xyz[3];
                applyMatrix(rgb2xyz, lin, xyz);

                float outLin[3];
                applyMatrix(xyz2rgb, xyz, outLin);

                float enc[3];
                enc[0] = fwdOETF(outLin[0]);
                enc[1] = fwdOETF(outLin[1]);
                enc[2] = fwdOETF(outLin[2]);

                float mapped[3];
                gamutFn(enc, mapped);

                dst[di + 0] = orig[0] + mixF * (mapped[0] - orig[0]);
                dst[di + 1] = orig[1] + mixF * (mapped[1] - orig[1]);
                dst[di + 2] = orig[2] + mixF * (mapped[2] - orig[2]);
                dst[di + 3] = orig[3];
            }
        }

        effect->imageClipReleaseImage(srcData);
        effect->imageClipReleaseImage(dstData);
        return kOfxStatOK;
    }

    OfxStatus isIdentity(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs,
                         OfxPropertySetHandle outArgs) override
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

        auto getInt = [&](const char *name, int &v) -> bool {
            OfxParamSetHandle p;
            if (param->paramGetHandle(paramSet, name, &p, nullptr) != kOfxStatOK) return false;
            return param->paramGetValue(p, 0, &v) == kOfxStatOK;
        };

        int inputCS = kCS_Rec709, outputCS = kCS_sRGB;
        getInt(kParamInputCS, inputCS);
        getInt(kParamOutputCS, outputCS);
        inputCS = std::max(0, std::min(kCS_Count - 1, inputCS));
        outputCS = std::max(0, std::min(kCS_Count - 1, outputCS));

        if (inputCS == outputCS) {
            prop->propSetString(outArgs, kOfxImageEffectPropIsIdentityClip, 0, kClipSource);
            return kOfxStatOK;
        }
        return kOfxStatReplyDefault;
    }
};

static ColorSpacePlugin s_plugin;
static PluginRegistrar s_registrar(&s_plugin);
