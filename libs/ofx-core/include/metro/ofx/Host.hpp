// Copyright (c) 2026 Metro Design. All rights reserved.
#ifndef METRO_OFX_HOST_HPP
#define METRO_OFX_HOST_HPP

#include <string>
#include <memory>
#include <ofxCore.h>
#include <ofxProperty.h>
#include <ofxImageEffect.h>
#include <ofxParam.h>
#include <ofxMemory.h>

namespace metro::ofx {

struct HostInfo {
    std::string name;
    std::string label;
    std::string versionString;
    int versionMajor{0};
    int versionMinor{0};
    double frameRate{24.0};
    bool supportsMips{false};
    double pixelScaleX{1.0};
    double pixelScaleY{1.0};
};

using SuiteFetchProc = OfxStatus (*)(const char *suiteName, int suiteVersion, void **suitePtr);

class Host {
public:
    explicit Host(OfxHost *hostHandle);

    HostInfo probeCapabilities() const;

    OfxHost *handle() const { return hostHandle_; }

    const OfxPropertySuiteV1      *properties() const { return propSuite_; }
    const OfxImageEffectSuiteV1   *imageEffect() const { return imageEffectSuite_; }
    const OfxParamSuiteV1         *parameters() const { return paramSuite_; }
    const OfxMemorySuiteV1        *memory() const { return memorySuite_; }

    bool isValid() const { return hostHandle_ != nullptr; }
    bool suitesAvailable() const {
        return propSuite_ && imageEffectSuite_ && paramSuite_;
    }

private:
    OfxHost *hostHandle_{nullptr};
    SuiteFetchProc fetchSuite_{nullptr};

    const OfxPropertySuiteV1       *propSuite_{nullptr};
    const OfxImageEffectSuiteV1    *imageEffectSuite_{nullptr};
    const OfxParamSuiteV1          *paramSuite_{nullptr};
    const OfxMemorySuiteV1         *memorySuite_{nullptr};
};

} // namespace metro::ofx

#endif
