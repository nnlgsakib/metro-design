// Copyright (c) 2026 Metro Design. All rights reserved.
#include "metro/ofx/Plugin.hpp"
#include "metro/ofx/param/ParamManager.hpp"
#include <cstring>
#include <cstdio>

using namespace metro::ofx;
using namespace metro::ofx::param;

class SamplePlugin : public Plugin {
public:
    const char *identifier() const override { return "com.metrodesign.sampleplugin"; }
    const char *label() const override { return "Metro Sample Plugin"; }
    const char *description() const override {
        return "A minimal Metro Design OFX plugin demonstrating the bootstrap infrastructure.";
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
        prop->propSetString(props, kOfxImageEffectPropShortLabel, 0, "MetroSample");
        prop->propSetString(props, kOfxImageEffectPropLongLabel, 0, "Metro Design Sample Plugin");
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

        OfxParamSetHandle paramSet;
        OfxStatus stat = host().imageEffect()->getParamSet(descriptor, &paramSet);
        if (stat != kOfxStatOK) return stat;

        OfxPropertySetHandle paramProps;
        const OfxParamSuiteV1 *param = host().parameters();

        stat = param->paramDefine(paramSet, kOfxParamTypeDouble, "gain", &paramProps);
        if (stat != kOfxStatOK) return stat;

        if (host().properties()) {
            auto *prop = host().properties();
            prop->propSetString(paramProps, kOfxParamPropLabel, 0, "Gain");
            prop->propSetString(paramProps, kOfxParamPropHint, 0, "Multiplicative gain factor");
            prop->propSetDouble(paramProps, kOfxParamPropDoubleMin, 0, 0.0);
            prop->propSetDouble(paramProps, kOfxParamPropDoubleMax, 0, 10.0);
            prop->propSetDouble(paramProps, kOfxParamPropDoubleDefault, 0, 1.0);
            prop->propSetDouble(paramProps, kOfxParamPropIncrement, 0, 0.1);
            prop->propSetInt(paramProps, kOfxParamPropDigits, 0, 3);
        }

        return kOfxStatOK;
    }

    OfxStatus render(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) override
    {
        (void)instance;
        (void)inArgs;
        (void)outArgs;

        if (!hostAvailable()) return kOfxStatErrBadHandle;

        auto info = host().probeCapabilities();
        std::printf("[MetroSample] Rendering frame in host: %s v%s\n",
                    info.name.c_str(), info.versionString.c_str());

        return kOfxStatOK;
    }
};

static SamplePlugin s_samplePlugin;
static PluginRegistrar s_registrar(&s_samplePlugin);
