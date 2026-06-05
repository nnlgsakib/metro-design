// Copyright (c) 2026 Metro Design. All rights reserved.
#include "metro/ofx/Plugin.hpp"
#include "metro/ofx/param/ParamManager.hpp"
#include <cstring>
#include <cstdio>

namespace metro::ofx {

Plugin::~Plugin() = default;

OfxStatus Plugin::setHost(OfxHost *host)
{
    host_ = std::make_unique<Host>(host);
    return host_ ? kOfxStatOK : kOfxStatErrMemory;
}

OfxStatus Plugin::describe(OfxImageEffectHandle) { return kOfxStatReplyDefault; }
OfxStatus Plugin::describeInContext(OfxImageEffectHandle, int) { return kOfxStatReplyDefault; }
OfxStatus Plugin::createInstance(OfxImageEffectHandle) { return kOfxStatReplyDefault; }
OfxStatus Plugin::destroyInstance(OfxImageEffectHandle) { return kOfxStatReplyDefault; }
OfxStatus Plugin::render(OfxImageEffectHandle, OfxPropertySetHandle, OfxPropertySetHandle) { return kOfxStatReplyDefault; }
OfxStatus Plugin::isIdentity(OfxImageEffectHandle, OfxPropertySetHandle, OfxPropertySetHandle) { return kOfxStatReplyDefault; }
OfxStatus Plugin::clipPreferences(OfxImageEffectHandle, OfxPropertySetHandle) { return kOfxStatReplyDefault; }
OfxStatus Plugin::getRegionOfDefinition(OfxImageEffectHandle, OfxPropertySetHandle, OfxPropertySetHandle) { return kOfxStatReplyDefault; }
OfxStatus Plugin::instanceChanged(OfxImageEffectHandle, OfxPropertySetHandle, OfxPropertySetHandle) { return kOfxStatReplyDefault; }
OfxStatus Plugin::beginRender(OfxImageEffectHandle, OfxPropertySetHandle) { return kOfxStatReplyDefault; }
OfxStatus Plugin::endRender(OfxImageEffectHandle, OfxPropertySetHandle) { return kOfxStatReplyDefault; }

static const int kMaxPlugins = 32;
static int s_pluginCount = 0;
static Plugin *s_plugins[kMaxPlugins] = {};
static OfxPlugin s_pluginDefs[kMaxPlugins] = {};

static int pluginIndexForHandle(void *handle)
{
    for (int i = 0; i < s_pluginCount; ++i) {
        if (s_plugins[i] && s_plugins[i]->instanceHandle() == handle)
            return i;
    }
    return -1;
}

extern "C" {

__attribute__((visibility("default")))
int OfxGetNumberOfPlugins(void)
{
    return s_pluginCount;
}

__attribute__((visibility("default")))
OfxPlugin *OfxGetPlugin(int index)
{
    if (index < 0 || index >= s_pluginCount)
        return nullptr;
    return &s_pluginDefs[index];
}

} // extern "C"

static OfxStatus pluginSetHostCallback(OfxHost *host)
{
    for (int i = 0; i < s_pluginCount; ++i) {
        if (s_plugins[i]) {
            OfxStatus stat = s_plugins[i]->setHost(host);
            if (stat != kOfxStatOK) return stat;
        }
    }
    return kOfxStatOK;
}

static OfxStatus pluginMainEntryCallback(const char *action, void *handle, void *inArgs, void *outArgs)
{
    if (!handle) return kOfxStatErrBadHandle;

    Plugin *plugin = nullptr;

    if (std::strcmp(action, kOfxActionDescribe) == 0) {
        if (s_pluginCount > 0)
            plugin = s_plugins[0];
    } else {
        int idx = pluginIndexForHandle(handle);
        if (idx >= 0) plugin = s_plugins[idx];
    }

    if (!plugin) return kOfxStatFailed;

    if (std::strcmp(action, kOfxActionDescribe) == 0) {
        return plugin->describe(static_cast<OfxImageEffectHandle>(handle));
    }
    if (std::strcmp(action, kOfxActionCreateInstance) == 0) {
        return plugin->createInstance(static_cast<OfxImageEffectHandle>(handle));
    }
    if (std::strcmp(action, kOfxActionDestroyInstance) == 0) {
        return plugin->destroyInstance(static_cast<OfxImageEffectHandle>(handle));
    }
    if (std::strcmp(action, kOfxActionRender) == 0) {
        return plugin->render(static_cast<OfxImageEffectHandle>(handle),
                              static_cast<OfxPropertySetHandle>(inArgs),
                              static_cast<OfxPropertySetHandle>(outArgs));
    }
    if (std::strcmp(action, kOfxActionIsIdentity) == 0) {
        return plugin->isIdentity(static_cast<OfxImageEffectHandle>(handle),
                                  static_cast<OfxPropertySetHandle>(inArgs),
                                  static_cast<OfxPropertySetHandle>(outArgs));
    }
    if (std::strcmp(action, kOfxImageEffectActionGetClipPreferences) == 0) {
        return plugin->clipPreferences(static_cast<OfxImageEffectHandle>(handle),
                                       static_cast<OfxPropertySetHandle>(outArgs));
    }
    if (std::strcmp(action, kOfxActionBeginRender) == 0) {
        return plugin->beginRender(static_cast<OfxImageEffectHandle>(handle),
                                   static_cast<OfxPropertySetHandle>(inArgs));
    }
    if (std::strcmp(action, kOfxActionEndRender) == 0) {
        return plugin->endRender(static_cast<OfxImageEffectHandle>(handle),
                                 static_cast<OfxPropertySetHandle>(inArgs));
    }
    return kOfxStatReplyDefault;
}

static void registerPlugin(Plugin *plugin)
{
    if (s_pluginCount >= kMaxPlugins) return;

    int idx = s_pluginCount;
    s_plugins[idx] = plugin;

    OfxPlugin &def = s_pluginDefs[idx];
    def.next = nullptr;
    def.identifier = plugin->identifier();
    def.pluginVersion = plugin->versionString();
    def.apiVersion = "OfxImageEffectPluginAPI";
    def.apiMinor = 1;
    def.setHost = pluginSetHostCallback;
    def.mainEntry = pluginMainEntryCallback;

    s_pluginCount++;
}

PluginRegistrar::PluginRegistrar(Plugin *plugin)
{
    registerPlugin(plugin);
}

} // namespace metro::ofx
