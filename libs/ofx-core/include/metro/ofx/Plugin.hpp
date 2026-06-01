#ifndef METRO_OFX_PLUGIN_HPP
#define METRO_OFX_PLUGIN_HPP

#include <string>
#include <memory>
#include <ofxCore.h>

#include "Host.hpp"

namespace metro::ofx {

class Plugin {
public:
    Plugin() = default;
    virtual ~Plugin() = default;

    Plugin(const Plugin &) = delete;
    Plugin &operator=(const Plugin &) = delete;

    virtual const char *identifier() const = 0;
    virtual const char *label() const = 0;
    virtual const char *description() const { return ""; }
    virtual const char *versionString() const = 0;
    virtual int apiVersionMajor() const { return 1; }
    virtual int apiVersionMinor() const { return 0; }
    virtual const char *pluginGrouping() const { return ""; }

    virtual OfxStatus setHost(OfxHost *host);
    virtual OfxStatus describe(OfxImageEffectHandle descriptor);
    virtual OfxStatus describeInContext(OfxImageEffectHandle descriptor, int contextIndex);
    virtual OfxStatus createInstance(OfxImageEffectHandle instance);
    virtual OfxStatus destroyInstance(OfxImageEffectHandle instance);
    virtual OfxStatus render(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs);
    virtual OfxStatus isIdentity(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs);
    virtual OfxStatus clipPreferences(OfxImageEffectHandle instance, OfxPropertySetHandle outArgs);
    virtual OfxStatus getRegionOfDefinition(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs);
    virtual OfxStatus instanceChanged(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs);
    virtual OfxStatus beginRender(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs);
    virtual OfxStatus endRender(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs);

    const Host &host() const { return *host_; }
    Host &host() { return *host_; }
    bool hostAvailable() const { return host_ != nullptr; }
    OfxImageEffectHandle instanceHandle() const { return instanceHandle_; }

protected:
    std::unique_ptr<Host> host_;
    OfxImageEffectHandle instanceHandle_{nullptr};
};

struct PluginRegistrar {
    explicit PluginRegistrar(Plugin *plugin);
};

} // namespace metro::ofx

#endif
