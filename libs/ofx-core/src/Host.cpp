// Copyright (c) 2026 Metro Design. All rights reserved.
#include "metro/ofx/Host.hpp"
#include <cstring>

#define kOfxHostSuiteFetchPtr "OfxHostSuiteFetchPtr"

namespace metro::ofx {

Host::Host(OfxHost *hostHandle)
    : hostHandle_(hostHandle)
{
    if (!hostHandle_ || !hostHandle_->host) return;

    OfxPropertySetHandle hostProps = hostHandle_->host;

    void *fetchRaw = nullptr;
    if (hostProps) {
        const char *type = nullptr;
        if (propSuite_) {
            if (propSuite_->propGetString(hostProps, kOfxPropType, 0, &type) == kOfxStatOK && type) {
                if (std::strcmp(type, kOfxTypeProperty) == 0) {
                    void *raw = nullptr;
                    propSuite_->propGetPointer(hostProps, kOfxHostSuiteFetchPtr, 0, &raw);
                    fetchRaw = raw;
                }
            }
        }
    }

    fetchSuite_ = reinterpret_cast<SuiteFetchProc>(fetchRaw);

    if (fetchSuite_) {
        void *tmp = nullptr;
        if (fetchSuite_(kOfxPropertySuite, kOfxPropertySuiteVersion, &tmp) == kOfxStatOK)
            propSuite_ = static_cast<const OfxPropertySuiteV1*>(tmp);

        tmp = nullptr;
        if (fetchSuite_(kOfxImageEffectSuite, kOfxImageEffectSuiteVersion, &tmp) == kOfxStatOK)
            imageEffectSuite_ = static_cast<const OfxImageEffectSuiteV1*>(tmp);

        tmp = nullptr;
        if (fetchSuite_(kOfxParamSuite, kOfxParamSuiteVersion, &tmp) == kOfxStatOK)
            paramSuite_ = static_cast<const OfxParamSuiteV1*>(tmp);

        tmp = nullptr;
        if (fetchSuite_(kOfxMemorySuite, kOfxMemorySuiteVersion, &tmp) == kOfxStatOK)
            memorySuite_ = static_cast<const OfxMemorySuiteV1*>(tmp);
    }
}

HostInfo Host::probeCapabilities() const
{
    HostInfo info;
    if (!hostHandle_ || !hostHandle_->host || !propSuite_) return info;

    OfxPropertySetHandle hostProps = hostHandle_->host;

    const char *name = nullptr;
    if (propSuite_->propGetString(hostProps, kOfxPropName, 0, &name) == kOfxStatOK && name)
        info.name = name;

    const char *label = nullptr;
    if (propSuite_->propGetString(hostProps, kOfxPropLabel, 0, &label) == kOfxStatOK && label)
        info.label = label;

    const char *ver = nullptr;
    if (propSuite_->propGetString(hostProps, kOfxPropVersion, 0, &ver) == kOfxStatOK && ver)
        info.versionString = ver;

    propSuite_->propGetInt(hostProps, "OfxPropVersionMajor", 0, &info.versionMajor);
    propSuite_->propGetInt(hostProps, "OfxPropVersionMinor", 0, &info.versionMinor);

    double fr = 24.0;
    if (propSuite_->propGetDouble(hostProps, kOfxImageEffectPropHostFrameRate, 0, &fr) == kOfxStatOK)
        info.frameRate = fr;

    return info;
}

} // namespace metro::ofx
