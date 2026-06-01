#include "metro/ofx/Plugin.hpp"
#include "metro/ofx/param/ParamManager.hpp"
#include "metro/blobtrack/BlobDetector.hpp"

#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <vector>

#define kOfxImageEffectPropData         "OfxImageEffectPropData"
#define kOfxImageEffectPropBounds       "OfxImageEffectPropBounds"
#define kOfxImageEffectPropRowBytes     "OfxImageEffectPropRowBytes"
#define kOfxImageEffectPropRenderTime   "OfxImageEffectPropRenderTime"

using namespace metro::ofx;
using namespace metro::ofx::param;
using namespace metro::blobtrack;

static const char *kParamEnable        = "enable";
static const char *kParamThreshold     = "threshold";
static const char *kParamMinBlobSize   = "minBlobSize";
static const char *kParamMaxBlobSize   = "maxBlobSize";
static const char *kParamProxMerge     = "proximityMerge";
static const char *kParamShowOverlay   = "showOverlay";
static const char *kParamShowTrails    = "showTrails";
static const char *kParamTrailLength   = "trailLength";
static const char *kParamOverlayOpacity = "overlayOpacity";
static const char *kParamOpacity       = "opacity";

class BlobTrackPlugin : public Plugin {
public:
    const char *identifier() const override { return "com.metrodesign.blobtrack"; }
    const char *label() const override { return "Metro Blob Tracker"; }
    const char *description() const override {
        return "Detect and track blobs (connected components) across frames "
               "with centroid tracking, bounding box overlay, and motion trails.";
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
        prop->propSetString(props, kOfxImageEffectPropShortLabel, 0, "MetroBlobTrk");
        prop->propSetString(props, kOfxImageEffectPropLongLabel, 0, "Metro Design Blob Tracker");
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

        auto *param = host().parameters();
        auto *prop = host().properties();

        auto defParam = [&](const char *name, const char *type) -> OfxStatus {
            OfxPropertySetHandle p;
            OfxStatus s = param->paramDefine(paramSet, type, name, &p);
            if (s != kOfxStatOK) return s;
            return kOfxStatOK;
        };

        stat = defParam(kParamEnable, kOfxParamTypeBoolean);
        if (stat != kOfxStatOK) return stat;

        stat = defParam(kParamThreshold, kOfxParamTypeDouble);
        if (stat != kOfxStatOK) return stat;

        stat = defParam(kParamMinBlobSize, kOfxParamTypeInteger);
        if (stat != kOfxStatOK) return stat;

        stat = defParam(kParamMaxBlobSize, kOfxParamTypeInteger);
        if (stat != kOfxStatOK) return stat;

        stat = defParam(kParamProxMerge, kOfxParamTypeDouble);
        if (stat != kOfxStatOK) return stat;

        stat = defParam(kParamShowOverlay, kOfxParamTypeBoolean);
        if (stat != kOfxStatOK) return stat;

        stat = defParam(kParamShowTrails, kOfxParamTypeBoolean);
        if (stat != kOfxStatOK) return stat;

        stat = defParam(kParamTrailLength, kOfxParamTypeInteger);
        if (stat != kOfxStatOK) return stat;

        stat = defParam(kParamOverlayOpacity, kOfxParamTypeDouble);
        if (stat != kOfxStatOK) return stat;

        stat = defParam(kParamOpacity, kOfxParamTypeDouble);
        if (stat != kOfxStatOK) return stat;

        return kOfxStatOK;
    }

    OfxStatus createInstance(OfxImageEffectHandle instance) override
    {
        instanceHandle_ = instance;
        detector_ = new BlobDetector();
        return kOfxStatOK;
    }

    OfxStatus destroyInstance(OfxImageEffectHandle instance) override
    {
        (void)instance;
        delete detector_;
        detector_ = nullptr;
        instanceHandle_ = nullptr;
        return kOfxStatOK;
    }

    OfxStatus isIdentity(OfxImageEffectHandle instance,
                         OfxPropertySetHandle inArgs,
                         OfxPropertySetHandle outArgs) override
    {
        (void)instance;
        (void)inArgs;
        (void)outArgs;
        if (!hostAvailable() || !host().properties() || !host().imageEffect())
            return kOfxStatErrBadHandle;

        int enabled = getParamIntDefault(kParamEnable, 1);
        if (enabled == 0) return kOfxStatReplyYes;

        return kOfxStatReplyDefault;
    }

    OfxStatus render(OfxImageEffectHandle instance,
                     OfxPropertySetHandle inArgs,
                     OfxPropertySetHandle outArgs) override
    {
        (void)outArgs;

        if (!hostAvailable()) return kOfxStatErrBadHandle;

        auto *ie = host().imageEffect();
        auto *prop = host().properties();
        if (!ie || !prop) return kOfxStatErrBadHandle;

        double time = 0.0;
        prop->propGetDouble(inArgs, kOfxImageEffectPropRenderTime, 0, &time);

        OfxImageEffectHandle srcClip = nullptr;
        OfxPropertySetHandle srcClipProps = nullptr;
        OfxStatus stat = ie->clipGetHandle(instance, "Source", &srcClip, &srcClipProps);
        if (stat != kOfxStatOK || !srcClip) return kOfxStatErrBadHandle;

        OfxImageEffectHandle outClip = nullptr;
        OfxPropertySetHandle outClipProps = nullptr;
        stat = ie->clipGetHandle(instance, "Output", &outClip, &outClipProps);
        if (stat != kOfxStatOK || !outClip) return kOfxStatErrBadHandle;

        OfxPropertySetHandle srcImage = nullptr;
        OfxPropertySetHandle srcData = nullptr;
        stat = ie->imageClipGetImage(srcClip, time, nullptr, &srcImage, &srcData);
        if (stat != kOfxStatOK || !srcImage || !srcData) return kOfxStatOK;

        OfxPropertySetHandle dstImage = nullptr;
        OfxPropertySetHandle dstData = nullptr;
        stat = ie->imageClipGetImage(outClip, time, nullptr, &dstImage, &dstData);
        if (stat != kOfxStatOK || !dstImage || !dstData) {
            ie->imageClipReleaseImage(srcData);
            return kOfxStatOK;
        }

        void *srcPtr = nullptr;
        void *dstPtr = nullptr;
        int srcRowBytes = 0, dstRowBytes = 0;
        int srcBounds[4] = {0, 0, 0, 0};

        prop->propGetPointer(srcData, kOfxImageEffectPropData, 0, &srcPtr);
        prop->propGetPointer(dstData, kOfxImageEffectPropData, 0, &dstPtr);
        prop->propGetInt(srcData, kOfxImageEffectPropRowBytes, 0, &srcRowBytes);
        prop->propGetInt(dstData, kOfxImageEffectPropRowBytes, 0, &dstRowBytes);
        prop->propGetIntN(srcData, kOfxImageEffectPropBounds, 4, srcBounds);

        int srcW = srcBounds[2] - srcBounds[0];
        int srcH = srcBounds[3] - srcBounds[1];
        if (!srcPtr || !dstPtr || srcW <= 0 || srcH <= 0) {
            ie->imageClipReleaseImage(srcData);
            ie->imageClipReleaseImage(dstData);
            return kOfxStatErrBadHandle;
        }

        int w = srcW;
        int h = srcH;

        auto *src = static_cast<const uint8_t *>(srcPtr);
        auto *dst = static_cast<uint8_t *>(dstPtr);
        for (int y = 0; y < h; ++y) {
            std::memcpy(dst + y * dstRowBytes, src + y * srcRowBytes,
                        static_cast<size_t>(w) * 4);
        }

        BlobParams bp;
        bp.threshold = static_cast<float>(getParamDoubleDefault(kParamThreshold, 0.5));
        bp.minBlobArea = getParamIntDefault(kParamMinBlobSize, 10);
        bp.maxBlobArea = getParamIntDefault(kParamMaxBlobSize, 5000);
        bp.proximityMerge = static_cast<float>(getParamDoubleDefault(kParamProxMerge, 10.0));
        bp.enabled = getParamIntDefault(kParamEnable, 1) != 0;

        int showOverlay = getParamIntDefault(kParamShowOverlay, 1);
        int showTrails = getParamIntDefault(kParamShowTrails, 1);
        int trailLen = getParamIntDefault(kParamTrailLength, 16);
        double overlayOpacity = getParamDoubleDefault(kParamOverlayOpacity, 0.5);
        double opacity = getParamDoubleDefault(kParamOpacity, 1.0);

        if (!bp.enabled) {
            ie->imageClipReleaseImage(srcData);
            ie->imageClipReleaseImage(dstData);
            return kOfxStatOK;
        }

        std::vector<uint8_t> gray(static_cast<size_t>(w) * h);
        for (int y = 0; y < h; ++y) {
            const auto *row = src + y * srcRowBytes;
            for (int x = 0; x < w; ++x) {
                int idx = x * 4;
                uint8_t r = row[idx];
                uint8_t g = row[idx + 1];
                uint8_t b = row[idx + 2];
                gray[static_cast<size_t>(y) * w + x] = static_cast<uint8_t>(
                    (static_cast<int>(r) * 77 +
                     static_cast<int>(g) * 150 +
                     static_cast<int>(b) * 29) >> 8);
            }
        }

        auto blobs = detector_->detect(gray.data(), w, h, w, bp);
        auto tracked = detector_->track(blobs);

        if (showOverlay && opacity > 0.01) {
            drawOverlay(dst, dstRowBytes, w, h, tracked,
                        showTrails != 0, trailLen,
                        static_cast<float>(overlayOpacity * opacity));
        }

        ie->imageClipReleaseImage(srcData);
        ie->imageClipReleaseImage(dstData);

        return kOfxStatOK;
    }

    OfxStatus clipPreferences(OfxImageEffectHandle instance,
                              OfxPropertySetHandle outArgs) override
    {
        (void)instance;
        (void)outArgs;
        return kOfxStatOK;
    }

    OfxStatus beginRender(OfxImageEffectHandle instance,
                          OfxPropertySetHandle inArgs) override
    {
        (void)instance;
        (void)inArgs;
        if (detector_) detector_->resetTracking();
        return kOfxStatOK;
    }

private:
    BlobDetector *detector_{nullptr};

    int getParamIntDefault(const char *name, int defaultValue) const
    {
        if (!hostAvailable() || !host().imageEffect() || !host().parameters())
            return defaultValue;
        OfxParamSetHandle ps;
        OfxStatus stat = host().imageEffect()->getParamSet(instanceHandle_, &ps);
        if (stat != kOfxStatOK) return defaultValue;

        auto *param = host().parameters();
        OfxParamSetHandle paramH = nullptr;
        OfxPropertySetHandle propH = nullptr;
        if (param->paramGetHandle(ps, name, &paramH, &propH) != kOfxStatOK || !paramH)
            return defaultValue;
        int val = defaultValue;
        param->paramGetValue(paramH, 0, &val);
        return val;
    }

    double getParamDoubleDefault(const char *name, double defaultValue) const
    {
        if (!hostAvailable() || !host().imageEffect() || !host().parameters())
            return defaultValue;
        OfxParamSetHandle ps;
        OfxStatus stat = host().imageEffect()->getParamSet(instanceHandle_, &ps);
        if (stat != kOfxStatOK) return defaultValue;

        auto *param = host().parameters();
        OfxParamSetHandle paramH = nullptr;
        OfxPropertySetHandle propH = nullptr;
        if (param->paramGetHandle(ps, name, &paramH, &propH) != kOfxStatOK || !paramH)
            return defaultValue;
        double val = defaultValue;
        param->paramGetValue(paramH, 0, &val);
        return val;
    }

    static void drawOverlay(uint8_t *dst, int dstRowBytes, int w, int h,
                            const std::vector<TrackedBlob> &tracked,
                            bool showTrails, int maxTrailLen, float opacity)
    {
        if (tracked.empty()) return;

        int alpha = static_cast<int>(opacity * 255.0f);
        alpha = std::max(0, std::min(255, alpha));

        for (const auto &tb : tracked) {
            int minX = std::max(0, tb.minX);
            int minY = std::max(0, tb.minY);
            int maxX = std::min(w - 1, tb.maxX);
            int maxY = std::min(h - 1, tb.maxY);

            for (int x = minX; x <= maxX; ++x) {
                if (minY >= 0 && minY < h) {
                    auto *p = dst + minY * dstRowBytes + x * 4;
                    p[1] = static_cast<uint8_t>((p[1] * (255 - alpha) + 255 * alpha) >> 8);
                }
                if (maxY >= 0 && maxY < h && maxY != minY) {
                    auto *p = dst + maxY * dstRowBytes + x * 4;
                    p[1] = static_cast<uint8_t>((p[1] * (255 - alpha) + 255 * alpha) >> 8);
                }
            }
            for (int y = minY; y <= maxY; ++y) {
                if (minX >= 0 && minX < w) {
                    auto *p = dst + y * dstRowBytes + minX * 4;
                    p[1] = static_cast<uint8_t>((p[1] * (255 - alpha) + 255 * alpha) >> 8);
                }
                if (maxX >= 0 && maxX < w && maxX != minX) {
                    auto *p = dst + y * dstRowBytes + maxX * 4;
                    p[1] = static_cast<uint8_t>((p[1] * (255 - alpha) + 255 * alpha) >> 8);
                }
            }

            int cx = static_cast<int>(tb.centroidX + 0.5f);
            int cy = static_cast<int>(tb.centroidY + 0.5f);
            for (int dx = -3; dx <= 3; ++dx) {
                int px = cx + dx;
                if (px >= 0 && px < w && cy >= 0 && cy < h) {
                    auto *p = dst + cy * dstRowBytes + px * 4;
                    p[0] = static_cast<uint8_t>((p[0] * (255 - alpha) + 255 * alpha) >> 8);
                }
            }
            for (int dy = -3; dy <= 3; ++dy) {
                int py = cy + dy;
                if (cx >= 0 && cx < w && py >= 0 && py < h) {
                    auto *p = dst + py * dstRowBytes + cx * 4;
                    p[0] = static_cast<uint8_t>((p[0] * (255 - alpha) + 255 * alpha) >> 8);
                }
            }

            if (!showTrails || tb.trailLen < 2) continue;
            int drawLen = std::min(tb.trailLen, maxTrailLen);
            int cap = TrackedBlob::kMaxTrail;
            for (int i = 0; i < drawLen - 1; ++i) {
                int idx = (tb.trailHead - drawLen + i + cap) % cap;
                int nxIdx = (tb.trailHead - drawLen + i + 1 + cap) % cap;
                int x1 = static_cast<int>(tb.trailX[idx] + 0.5f);
                int y1 = static_cast<int>(tb.trailY[idx] + 0.5f);
                int x2 = static_cast<int>(tb.trailX[nxIdx] + 0.5f);
                int y2 = static_cast<int>(tb.trailY[nxIdx] + 0.5f);

                int ldx = std::abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
                int ldy = std::abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
                int err = ldx - ldy;
                int cx2 = x1, cy2 = y1;
                while (true) {
                    if (cx2 >= 0 && cx2 < w && cy2 >= 0 && cy2 < h) {
                        auto *p = dst + cy2 * dstRowBytes + cx2 * 4;
                        p[2] = static_cast<uint8_t>((p[2] * (255 - alpha) + 255 * alpha) >> 8);
                    }
                    if (cx2 == x2 && cy2 == y2) break;
                    int e2 = 2 * err;
                    if (e2 > -ldy) { err -= ldy; cx2 += sx; }
                    if (e2 < ldx) { err += ldx; cy2 += sy; }
                }
            }
        }
    }
};

static BlobTrackPlugin s_blobTrackPlugin;
static PluginRegistrar s_registrar(&s_blobTrackPlugin);
