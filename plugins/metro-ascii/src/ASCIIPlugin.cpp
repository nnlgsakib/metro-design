#include "metro/ofx/Plugin.hpp"
#include "metro/ofx/param/ParamManager.hpp"
#include <cstring>
#include <cmath>
#include <cstdio>

static const char *kCharsetDefault = " .:-=+*#%@";
static const int   kCharsetLen = 10;

#define kOfxImageEffectPropData         "OfxImageEffectPropData"
#define kOfxImageEffectPropBounds       "OfxImageEffectPropBounds"
#define kOfxImageEffectPropRegionOfDefinition "OfxImageEffectPropRegionOfDefinition"
#define kOfxImageEffectPropPixelDepth   "OfxImageEffectPropPixelDepth"
#define kOfxImageEffectPropComponentCount "OfxImageEffectPropComponentCount"
#define kOfxImageEffectPropRowBytes     "OfxImageEffectPropRowBytes"
#define kOfxImageEffectPropRenderScale  "OfxImageEffectPropRenderScale"
#define kOfxImageEffectPropComponents   "OfxImageEffectPropComponents"

using namespace metro::ofx;
using namespace metro::ofx::param;

static float luminance(float r, float g, float b)
{
    return 0.299f * r + 0.587f * g + 0.114f * b;
}

static char mapLuminance(float luma, const char *charset, int charsetLen)
{
    float clamped = luma < 0.0f ? 0.0f : (luma > 1.0f ? 1.0f : luma);
    int idx = static_cast<int>(clamped * (charsetLen - 1) + 0.5f);
    if (idx < 0) idx = 0;
    if (idx >= charsetLen) idx = charsetLen - 1;
    return charset[idx];
}

static void drawChar(float *outRow, int outWidth, int x, int y,
                     int cellW, int cellH, char ch, float aspect,
                     float charR, float charG, float charB)
{
    int density = static_cast<int>(ch == ' ' ? 0 : (ch - ' ') * 4);
    if (density > 255) density = 255;

    int cx = x * cellW;
    int cy = y * cellH;

    int charWidth = static_cast<int>(cellW * aspect);
    if (charWidth < 1) charWidth = 1;
    if (charWidth > cellW) charWidth = cellW;

    int strokeH = cellH / 5;
    if (strokeH < 1) strokeH = 1;

    int centerX = cx + (cellW - charWidth) / 2;
    int centerY = cy + cellH / 3;

    for (int dy = 0; dy < strokeH && (centerY + dy) < (cy + cellH); ++dy) {
        int py = centerY + dy;
        if (py >= cy + cellH) break;
        for (int dx = 0; dx < charWidth; ++dx) {
            int px = centerX + dx;
            if (px >= cx + cellW) break;
            if (px < 0 || px >= outWidth) continue;
            int outIdx = (py * outWidth + px) * 4;
            float blend = density / 255.0f;
            outRow[outIdx + 0] = outRow[outIdx + 0] * (1.0f - blend) + charR * blend;
            outRow[outIdx + 1] = outRow[outIdx + 1] * (1.0f - blend) + charG * blend;
            outRow[outIdx + 2] = outRow[outIdx + 2] * (1.0f - blend) + charB * blend;
            outRow[outIdx + 3] = 1.0f;
        }
    }
}

class ASCIIPlugin : public Plugin {
public:
    const char *identifier() const override { return "com.metrodesign.asciieffect"; }
    const char *label() const override { return "Metro ASCII Art"; }
    const char *description() const override {
        return "Real-time ASCII art conversion for video. Converts luminance to "
               "character density with configurable cell size, font aspect, and color passthrough.";
    }
    const char *versionString() const override { return "1.0.0"; }
    const char *pluginGrouping() const override { return "Metro Design"; }

    OfxStatus describe(OfxImageEffectHandle descriptor) override
    {
        if (!hostAvailable() || !host().properties()) return kOfxStatErrBadHandle;

        OfxPropertySetHandle props;
        OfxStatus stat = host().imageEffect()->getPropertySet(descriptor, &props);
        if (stat != kOfxStatOK) return stat;

        auto *prop = host().properties();
        prop->propSetString(props, kOfxImageEffectPropLabel, 0, label());
        prop->propSetString(props, kOfxImageEffectPropShortLabel, 0, "MetroASCII");
        prop->propSetString(props, kOfxImageEffectPropLongLabel, 0, "Metro Design ASCII Art");
        prop->propSetString(props, kOfxImageEffectPropGrouping, 0, pluginGrouping());
        prop->propSetString(props, kOfxImageEffectPropDescription, 0, description());

        const char *contexts[] = { kOfxImageEffectContextFilter, nullptr };
        prop->propSetStringN(props, kOfxImageEffectPropSupportedContexts, 1, contexts);

        return kOfxStatOK;
    }

    OfxStatus describeInContext(OfxImageEffectHandle descriptor, int contextIndex) override
    {
        (void)contextIndex;
        if (!hostAvailable() || !host().imageEffect() || !host().parameters())
            return kOfxStatErrBadHandle;

        OfxParamSetHandle paramSet;
        OfxStatus stat = host().imageEffect()->getParamSet(descriptor, &paramSet);
        if (stat != kOfxStatOK) return stat;

        auto *prop = host().properties();
        auto *param = host().parameters();

        if (!prop || !param) return kOfxStatErrBadHandle;

        {
            OfxPropertySetHandle p;
            stat = param->paramDefine(paramSet, kOfxParamTypeInteger, "cellSize", &p);
            if (stat != kOfxStatOK) return stat;
            prop->propSetString(p, kOfxParamPropLabel, 0, "Cell Size");
            prop->propSetString(p, kOfxParamPropHint, 0, "Pixels per ASCII cell");
            prop->propSetInt(p, kOfxParamPropIntMin, 0, 2);
            prop->propSetInt(p, kOfxParamPropIntMax, 0, 64);
            prop->propSetInt(p, kOfxParamPropIntDefault, 0, 8);
        }

        {
            OfxPropertySetHandle p;
            stat = param->paramDefine(paramSet, kOfxParamTypeDouble, "fontAspect", &p);
            if (stat != kOfxStatOK) return stat;
            prop->propSetString(p, kOfxParamPropLabel, 0, "Font Aspect");
            prop->propSetString(p, kOfxParamPropHint, 0, "Character width/height ratio");
            prop->propSetDouble(p, kOfxParamPropDoubleMin, 0, 0.2);
            prop->propSetDouble(p, kOfxParamPropDoubleMax, 0, 1.0);
            prop->propSetDouble(p, kOfxParamPropDoubleDefault, 0, 0.5);
            prop->propSetDouble(p, kOfxParamPropIncrement, 0, 0.05);
            prop->propSetInt(p, kOfxParamPropDigits, 0, 2);
        }

        {
            OfxPropertySetHandle p;
            stat = param->paramDefine(paramSet, kOfxParamTypeBoolean, "colorPassthrough", &p);
            if (stat != kOfxStatOK) return stat;
            prop->propSetString(p, kOfxParamPropLabel, 0, "Color Passthrough");
            prop->propSetString(p, kOfxParamPropHint, 0, "Use original frame colors");
            prop->propSetInt(p, kOfxParamPropIntDefault, 0, 1);
        }

        {
            OfxPropertySetHandle p;
            stat = param->paramDefine(paramSet, kOfxParamTypeDouble, "contrast", &p);
            if (stat != kOfxStatOK) return stat;
            prop->propSetString(p, kOfxParamPropLabel, 0, "Contrast");
            prop->propSetString(p, kOfxParamPropHint, 0, "Luminance contrast stretch");
            prop->propSetDouble(p, kOfxParamPropDoubleMin, 0, 0.0);
            prop->propSetDouble(p, kOfxParamPropDoubleMax, 0, 4.0);
            prop->propSetDouble(p, kOfxParamPropDoubleDefault, 0, 1.0);
            prop->propSetDouble(p, kOfxParamPropIncrement, 0, 0.1);
            prop->propSetInt(p, kOfxParamPropDigits, 0, 2);
        }

        return kOfxStatOK;
    }

    OfxStatus render(OfxImageEffectHandle instance,
                     OfxPropertySetHandle inArgs,
                     OfxPropertySetHandle outArgs) override
    {
        (void)outArgs;

        if (!hostAvailable()) return kOfxStatErrBadHandle;

        auto *imgEffect = host().imageEffect();
        auto *prop = host().properties();
        if (!imgEffect || !prop) return kOfxStatErrBadHandle;

        double time = 0.0;
        {
            const char *timeStr = nullptr;
            OfxPropertySetHandle renderProps = inArgs;
            if (prop->propGetString(renderProps, "OfxImageEffectPropRenderTime", 0, &timeStr) == kOfxStatOK && timeStr) {
                time = std::atof(timeStr);
            } else {
                prop->propGetDouble(inArgs, "OfxImageEffectPropTime", 0, &time);
            }
        }

        OfxImageEffectHandle srcClip = nullptr;
        OfxPropertySetHandle srcClipProps = nullptr;
        OfxStatus stat = imgEffect->clipGetHandle(instance, "Source", &srcClip, &srcClipProps);
        if (stat != kOfxStatOK || !srcClip) return kOfxStatErrBadHandle;

        OfxImageEffectHandle outClip = nullptr;
        OfxPropertySetHandle outClipProps = nullptr;
        stat = imgEffect->clipGetHandle(instance, "Output", &outClip, &outClipProps);
        if (stat != kOfxStatOK || !outClip) return kOfxStatErrBadHandle;

        OfxPropertySetHandle srcImage = nullptr;
        OfxPropertySetHandle srcData = nullptr;
        stat = imgEffect->imageClipGetImage(srcClip, time, nullptr, &srcImage, &srcData);
        if (stat != kOfxStatOK || !srcImage || !srcData) return kOfxStatOK;

        OfxPropertySetHandle dstImage = nullptr;
        OfxPropertySetHandle dstData = nullptr;
        stat = imgEffect->imageClipGetImage(outClip, time, nullptr, &dstImage, &dstData);
        if (stat != kOfxStatOK || !dstImage || !dstData) {
            imgEffect->imageClipReleaseImage(srcData);
            return kOfxStatOK;
        }

        void *srcPtr = nullptr;
        void *dstPtr = nullptr;
        prop->propGetPointer(srcData, kOfxImageEffectPropData, 0, &srcPtr);
        prop->propGetPointer(dstData, kOfxImageEffectPropData, 0, &dstPtr);

        if (!srcPtr || !dstPtr) {
            imgEffect->imageClipReleaseImage(srcData);
            imgEffect->imageClipReleaseImage(dstData);
            return kOfxStatOK;
        }

        int srcBounds[4] = {0, 0, 0, 0};
        int srcRowBytes = 0;
        int dstRowBytes = 0;
        prop->propGetIntN(srcData, kOfxImageEffectPropBounds, 4, srcBounds);
        prop->propGetInt(srcData, kOfxImageEffectPropRowBytes, 0, &srcRowBytes);
        prop->propGetInt(dstData, kOfxImageEffectPropRowBytes, 0, &dstRowBytes);

        int srcWidth = srcBounds[2] - srcBounds[1];
        int srcHeight = srcBounds[3] - srcBounds[0];

        int cellSize = 8;
        double fontAspect = 0.5;
        int colorPass = 1;
        double contrast = 1.0;

        {
            OfxParamSetHandle paramSet;
            if (imgEffect->getParamSet(instance, &paramSet) == kOfxStatOK && paramSet) {
                auto *param = host().parameters();
                if (param) {
                    OfxParamSetHandle paramH = nullptr;
                    OfxPropertySetHandle paramProps = nullptr;

                    if (param->paramGetHandle(paramSet, "cellSize", &paramH, &paramProps) == kOfxStatOK && paramH) {
                        param->paramGetValue(paramH, 0, &cellSize);
                    }

                    paramH = nullptr; paramProps = nullptr;
                    if (param->paramGetHandle(paramSet, "fontAspect", &paramH, &paramProps) == kOfxStatOK && paramH) {
                        param->paramGetValue(paramH, 0, &fontAspect);
                    }

                    paramH = nullptr; paramProps = nullptr;
                    if (param->paramGetHandle(paramSet, "colorPassthrough", &paramH, &paramProps) == kOfxStatOK && paramH) {
                        int val = 0;
                        param->paramGetValue(paramH, 0, &val);
                        colorPass = val;
                    }

                    paramH = nullptr; paramProps = nullptr;
                    if (param->paramGetHandle(paramSet, "contrast", &paramH, &paramProps) == kOfxStatOK && paramH) {
                        param->paramGetValue(paramH, 0, &contrast);
                    }
                }
            }
        }

        if (cellSize < 2) cellSize = 2;
        if (cellSize > 64) cellSize = 64;

        float *srcF = static_cast<float*>(srcPtr);
        float *dstF = static_cast<float*>(dstPtr);

        std::memcpy(dstF, srcF, srcHeight * dstRowBytes);

        int cols = srcWidth / cellSize;
        int rows = srcHeight / cellSize;

        float contrastScale = static_cast<float>(contrast);
        float contrastOffset = (1.0f - contrastScale) * 0.5f;

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
                float sumLuma = 0.0f;
                int count = 0;

                for (int dy = 0; dy < cellSize; ++dy) {
                    int py = row * cellSize + dy;
                    if (py >= srcHeight) break;
                    for (int dx = 0; dx < cellSize; ++dx) {
                        int px = col * cellSize + dx;
                        if (px >= srcWidth) break;

                        int idx = (py * srcWidth + px) * 4;
                        float r = srcF[idx + 0];
                        float g = srcF[idx + 1];
                        float b = srcF[idx + 2];

                        sumR += r;
                        sumG += g;
                        sumB += b;
                        sumLuma += luminance(r, g, b);
                        count++;
                    }
                }

                if (count == 0) continue;

                float avgR = sumR / count;
                float avgG = sumG / count;
                float avgB = sumB / count;
                float avgLuma = sumLuma / count;

                float stretchedLuma = (avgLuma - 0.5f) * contrastScale + 0.5f + contrastOffset;
                if (stretchedLuma < 0.0f) stretchedLuma = 0.0f;
                if (stretchedLuma > 1.0f) stretchedLuma = 1.0f;

                char ch = mapLuminance(stretchedLuma, kCharsetDefault, kCharsetLen);

                float charR = colorPass ? avgR : 1.0f;
                float charG = colorPass ? avgG : 1.0f;
                float charB = colorPass ? avgB : 1.0f;

                drawChar(dstF, srcWidth, col, row, cellSize, cellSize,
                         ch, static_cast<float>(fontAspect),
                         charR, charG, charB);
            }
        }

        imgEffect->imageClipReleaseImage(srcData);
        imgEffect->imageClipReleaseImage(dstData);

        return kOfxStatOK;
    }

    OfxStatus isIdentity(OfxImageEffectHandle instance,
                         OfxPropertySetHandle inArgs,
                         OfxPropertySetHandle outArgs) override
    {
        (void)inArgs;
        (void)outArgs;

        if (!hostAvailable() || !host().imageEffect() || !host().parameters())
            return kOfxStatReplyDefault;

        OfxParamSetHandle paramSet;
        OfxStatus stat = host().imageEffect()->getParamSet(instance, &paramSet);
        if (stat != kOfxStatOK) return kOfxStatReplyDefault;

        int cellSize = 8;

        auto *param = host().parameters();
        if (!param) return kOfxStatReplyDefault;

        OfxParamSetHandle p = nullptr;
        OfxPropertySetHandle pp = nullptr;

        if (param->paramGetHandle(paramSet, "cellSize", &p, &pp) == kOfxStatOK && p) {
            param->paramGetValue(p, 0, &cellSize);
        }
        if (cellSize == 0) return kOfxStatReplyYes;

        return kOfxStatReplyDefault;
    }
};

static ASCIIPlugin s_asciiPlugin;
static PluginRegistrar s_registrar(&s_asciiPlugin);
