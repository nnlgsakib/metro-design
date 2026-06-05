// Copyright (c) 2026 Metro Design. All rights reserved.
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <string>
#include <fstream>
#include <unordered_map>
#include <unistd.h>

#include <ofxCore.h>
#include <ofxProperty.h>
#include <ofxParam.h>
#include <ofxImageEffect.h>

#include "metro/ofx/Host.hpp"
#include "metro/ofx/Plugin.hpp"
#include "metro/ofx/param/ParamManager.hpp"
#include "metro/ofx/FeatureFlag.hpp"

using namespace metro::ofx;
using namespace metro::ofx::param;

static int s_testCount = 0;
static int s_passCount = 0;

#define TEST(name) do { \
    s_testCount++; \
    std::printf("  TEST: %s ... ", name); \
} while(0)

#define PASS() do { \
    s_passCount++; \
    std::printf("PASS\n"); \
} while(0)

#define FAIL(msg) do { \
    std::printf("FAIL: %s\n", msg); \
} while(0)

// ---------------------------------------------------------------------------
// Feature flag test helpers
// ---------------------------------------------------------------------------

static std::string s_tomlPath;

static void setup()
{
    char name[] = "/tmp/metro-ff-test-XXXXXX";
    int fd = mkstemp(name);
    if (fd != -1) close(fd);
    s_tomlPath = name;
}

static void teardown()
{
    if (!s_tomlPath.empty()) {
        std::remove(s_tomlPath.c_str());
    }
    FeatureFlagManager::instance().resetAll();
}

static void writeConfig(const std::string& content)
{
    std::ofstream out(s_tomlPath);
    assert(out.is_open());
    out << content;
    out.close();
}

void test_host_info_defaults()
{
    TEST("HostInfo default construction");

    HostInfo info;
    assert(info.name.empty());
    assert(info.label.empty());
    assert(info.versionString.empty());
    assert(info.versionMajor == 0);
    assert(info.versionMinor == 0);
    assert(info.frameRate == 24.0);

    PASS();
}

void test_host_null_handle()
{
    TEST("Host constructed with null handle");

    Host host(nullptr);
    assert(!host.isValid());
    assert(!host.suitesAvailable());
    assert(host.properties() == nullptr);
    assert(host.imageEffect() == nullptr);
    assert(host.parameters() == nullptr);
    assert(host.memory() == nullptr);

    HostInfo info = host.probeCapabilities();
    assert(info.name.empty());

    PASS();
}

void test_param_type_strings()
{
    TEST("param typeString covers all types");

    assert(std::strcmp(typeString(Type::Integer), kOfxParamTypeInteger) == 0);
    assert(std::strcmp(typeString(Type::Double), kOfxParamTypeDouble) == 0);
    assert(std::strcmp(typeString(Type::Boolean), kOfxParamTypeBoolean) == 0);
    assert(std::strcmp(typeString(Type::Choice), kOfxParamTypeChoice) == 0);
    assert(std::strcmp(typeString(Type::RGBA), kOfxParamTypeRGBA) == 0);
    assert(std::strcmp(typeString(Type::RGB), kOfxParamTypeRGB) == 0);
    assert(std::strcmp(typeString(Type::Double2D), kOfxParamTypeDouble2D) == 0);
    assert(std::strcmp(typeString(Type::Integer2D), kOfxParamTypeInteger2D) == 0);
    assert(std::strcmp(typeString(Type::Double3D), kOfxParamTypeDouble3D) == 0);
    assert(std::strcmp(typeString(Type::Integer3D), kOfxParamTypeInteger3D) == 0);
    assert(std::strcmp(typeString(Type::Group), kOfxParamTypeGroup) == 0);
    assert(std::strcmp(typeString(Type::Page), kOfxParamTypePage) == 0);
    assert(std::strcmp(typeString(Type::PushButton), kOfxParamTypePushButton) == 0);
    assert(std::strcmp(typeString(Type::String), kOfxParamTypeString) == 0);
    assert(std::strcmp(typeString(Type::Custom), kOfxParamTypeCustom) == 0);

    PASS();
}

void test_param_spec_defaults()
{
    TEST("ParamSpec default values");

    ParamSpec spec;
    assert(spec.type == Type::Double);
    assert(spec.name.empty());
    assert(spec.doubleMin == 0.0);
    assert(spec.doubleMax == 1.0);
    assert(spec.doubleDefault == 0.5);
    assert(spec.intMin == 0);
    assert(spec.intMax == 100);
    assert(spec.intDefault == 0);
    assert(spec.booleanDefault == false);
    assert(spec.evaluateOnChange == true);
    assert(spec.canInvalidate == false);

    PASS();
}

void test_ofx_core_constants()
{
    TEST("OFX core constants have expected values");

    assert(kOfxStatOK == 0);
    assert(kOfxStatFailed == -1);
    assert(kOfxStatErrBadHandle == -9);
    assert(kOfxStatReplyDefault == 2);

    assert(std::strcmp(kOfxPropertySuite, "OfxPropertySuiteV1") == 0);
    assert(std::strcmp(kOfxImageEffectSuite, "OfxImageEffectSuiteV1") == 0);
    assert(std::strcmp(kOfxParamSuite, "OfxParamSuiteV1") == 0);
    assert(std::strcmp(kOfxMemorySuite, "OfxMemorySuiteV1") == 0);

    assert(std::strcmp(kOfxImageEffectPluginApi, "OfxImageEffectPluginAPI") == 0);

    PASS();
}

void test_plugin_identifiers()
{
    TEST("OFX action identifiers are correct");

    assert(std::strcmp(kOfxActionLoad, "OfxActionLoad") == 0);
    assert(std::strcmp(kOfxActionUnload, "OfxActionUnload") == 0);
    assert(std::strcmp(kOfxActionDescribe, "OfxActionDescribe") == 0);
    assert(std::strcmp(kOfxActionCreateInstance, "OfxActionCreateInstance") == 0);
    assert(std::strcmp(kOfxActionDestroyInstance, "OfxActionDestroyInstance") == 0);
    assert(std::strcmp(kOfxActionRender, "OfxActionRender") == 0);
    assert(std::strcmp(kOfxActionIsIdentity, "OfxActionIsIdentity") == 0);

    PASS();
}

void test_image_effect_contexts()
{
    TEST("Image effect context identifiers are valid");

    assert(std::strcmp(kOfxImageEffectContextFilter, "OfxImageEffectContextFilter") == 0);
    assert(std::strcmp(kOfxImageEffectContextGeneral, "OfxImageEffectContextGeneral") == 0);
    assert(std::strcmp(kOfxImageEffectContextGenerator, "OfxImageEffectContextGenerator") == 0);
    assert(std::strcmp(kOfxImageEffectContextColorDecision, "OfxImageEffectContextColorDecision") == 0);

    PASS();
}

// ---------------------------------------------------------------------------
// Feature flag tests
// ---------------------------------------------------------------------------

void test_feature_flag_defaults()
{
    TEST("FeatureFlag default state");

    FeatureFlag f;
    assert(f.enabled() == false);
    assert(f.rolloutPercentage() == 100);
    assert(static_cast<bool>(f) == false);

    PASS();
}

void test_feature_flag_enable_disable()
{
    TEST("FeatureFlag enable/disable");

    FeatureFlag f;
    f.setEnabled(true);
    assert(f.enabled() == true);
    assert(static_cast<bool>(f) == true);

    f.setEnabled(false);
    assert(f.enabled() == false);

    PASS();
}

void test_feature_flag_rollout_clamping()
{
    TEST("FeatureFlag rollout percentage clamping");

    FeatureFlag f;
    f.setRolloutPercentage(-10);
    assert(f.rolloutPercentage() == 0);

    f.setRolloutPercentage(150);
    assert(f.rolloutPercentage() == 100);

    f.setRolloutPercentage(50);
    assert(f.rolloutPercentage() == 50);

    PASS();
}

void test_feature_flag_rollout_checks()
{
    TEST("FeatureFlag::checkRollout edge cases");

    assert(FeatureFlag::checkRollout(100, "anything") == true);
    assert(FeatureFlag::checkRollout(0, "anything") == false);

    bool foundEnabled = false;
    bool foundDisabled = false;
    for (int i = 0; i < 1000; ++i) {
        std::string name = "flag_" + std::to_string(i);
        if (FeatureFlag::checkRollout(50, name)) foundEnabled = true;
        else foundDisabled = true;
    }
    assert(foundEnabled);
    assert(foundDisabled);
    (void)foundEnabled;
    (void)foundDisabled;

    PASS();
}

void test_feature_flag_value_ctor()
{
    TEST("FeatureFlag value construction");

    FeatureFlag f1(true, 75);
    assert(f1.enabled() == true);
    assert(f1.rolloutPercentage() == 75);

    FeatureFlag f2(false, 0);
    assert(f2.enabled() == false);
    assert(f2.rolloutPercentage() == 0);

    PASS();
}

void test_manager_singleton()
{
    TEST("FeatureFlagManager singleton identity");

    auto& m1 = FeatureFlagManager::instance();
    auto& m2 = FeatureFlagManager::instance();
    assert(&m1 == &m2);
    (void)m1;
    (void)m2;

    PASS();
}

void test_manager_define_and_query()
{
    TEST("FeatureFlagManager define + isEnabled");

    auto& mgr = FeatureFlagManager::instance();

    FeatureFlag& f1 = mgr.define("testFlagA", true);
    assert(f1.enabled() == true);
    assert(mgr.isEnabled("testFlagA") == true);
    (void)f1;

    FeatureFlag& f2 = mgr.define("testFlagB", false);
    assert(f2.enabled() == false);
    assert(mgr.isEnabled("testFlagB") == false);
    (void)f2;

    mgr.define("testFlagC", true, 0);
    assert(mgr.isEnabled("testFlagC") == false);

    mgr.resetAll();
    PASS();
}

void test_manager_set_enabled()
{
    TEST("FeatureFlagManager setEnabled");

    auto& mgr = FeatureFlagManager::instance();
    mgr.define("toggleFlag", false);
    assert(mgr.isEnabled("toggleFlag") == false);

    mgr.setEnabled("toggleFlag", true);
    assert(mgr.isEnabled("toggleFlag") == true);

    mgr.setEnabled("toggleFlag", false);
    assert(mgr.isEnabled("toggleFlag") == false);

    mgr.resetAll();
    PASS();
}

void test_manager_reset_all()
{
    TEST("FeatureFlagManager resetAll");

    auto& mgr = FeatureFlagManager::instance();
    mgr.define("resetFlag", true);
    mgr.setEnabled("resetFlag", false);
    assert(mgr.isEnabled("resetFlag") == false);

    mgr.resetAll();
    assert(mgr.isEnabled("resetFlag") == true);

    PASS();
}

void test_manager_unknown_flag()
{
    TEST("FeatureFlagManager unknown flag returns false");

    assert(FeatureFlagManager::instance().isEnabled("nonexistentFlag") == false);

    PASS();
}

class TestOverrideProvider : public RemoteOverrideProvider {
public:
    explicit TestOverrideProvider(std::unordered_map<std::string, bool> overrides)
        : overrides_(std::move(overrides)) {}

    std::unordered_map<std::string, bool> fetchOverrides() override
    {
        return overrides_;
    }

    bool connected() const noexcept override { return true; }

private:
    std::unordered_map<std::string, bool> overrides_;
};

void test_manager_remote_override()
{
    TEST("FeatureFlagManager remote override");

    auto& mgr = FeatureFlagManager::instance();
    mgr.define("remoteFlag", false);
    assert(mgr.isEnabled("remoteFlag") == false);

    auto provider = std::make_unique<TestOverrideProvider>(
        std::unordered_map<std::string, bool>{{"remoteFlag", true}}
    );
    mgr.setRemoteOverride(std::move(provider));
    mgr.refresh();
    assert(mgr.isEnabled("remoteFlag") == true);

    mgr.resetAll();
    mgr.setRemoteOverride(nullptr);
    PASS();
}

void test_manager_env_var_override()
{
    TEST("FeatureFlagManager env var override");

    auto& mgr = FeatureFlagManager::instance();
    mgr.define("envTestFlag", false);
    assert(mgr.isEnabled("envTestFlag") == false);

    std::string envName = "METRO_FF_ENVTESTFLAG";
#ifdef _WIN32
    _putenv_s(envName.c_str(), "1");
#else
    setenv(envName.c_str(), "1", 1);
#endif

    mgr.refresh();
    assert(mgr.isEnabled("envTestFlag") == true);

#ifdef _WIN32
    _putenv_s(envName.c_str(), "");
#else
    unsetenv(envName.c_str());
#endif
    mgr.resetAll();

    PASS();
}

void test_manager_toml_config()
{
    TEST("FeatureFlagManager TOML config loading");

    auto& mgr = FeatureFlagManager::instance();
    mgr.define("tomlFlagA", false);
    mgr.define("tomlFlagB", true);

    writeConfig(
        "[flags]\n"
        "tomlFlagA = true\n"
        "tomlFlagB = false\n"
    );

    assert(mgr.loadConfigFile(s_tomlPath) == true);
    assert(mgr.isEnabled("tomlFlagA") == true);
    assert(mgr.isEnabled("tomlFlagB") == false);

    mgr.resetAll();
    PASS();
}

void test_manager_toml_config_missing_file()
{
    TEST("FeatureFlagManager TOML config missing file");

    assert(FeatureFlagManager::instance().loadConfigFile("/nonexistent/config.toml") == false);

    PASS();
}

void test_manager_toml_config_with_comments()
{
    TEST("FeatureFlagManager TOML config with comments");

    auto& mgr = FeatureFlagManager::instance();
    mgr.define("commentedFlag", false);
    mgr.define("anotherFlag", true);

    writeConfig(
        "# This is a comment\n"
        "[flags]\n"
        "commentedFlag = true  # inline comment\n"
        "anotherFlag = false\n"
    );

    mgr.loadConfigFile(s_tomlPath);
    assert(mgr.isEnabled("commentedFlag") == true);
    assert(mgr.isEnabled("anotherFlag") == false);

    mgr.resetAll();
    PASS();
}

void test_manager_toml_config_skip_other_sections()
{
    TEST("FeatureFlagManager TOML config skips other sections");

    auto& mgr = FeatureFlagManager::instance();
    mgr.define("sectionFlag", false);

    writeConfig(
        "[display]\n"
        "brightness = 50\n"
        "[flags]\n"
        "sectionFlag = true\n"
    );

    mgr.loadConfigFile(s_tomlPath);
    assert(mgr.isEnabled("sectionFlag") == true);

    mgr.resetAll();
    PASS();
}

void test_feature_flag_thread_safe()
{
    TEST("FeatureFlagManager thread-safe reads");

    auto& mgr = FeatureFlagManager::instance();
    mgr.define("threadFlag", true);
    assert(mgr.isEnabled("threadFlag") == true);

    mgr.resetAll();
    PASS();
}

void test_manager_env_var_false_values()
{
    TEST("FeatureFlagManager env var false values");

    auto& mgr = FeatureFlagManager::instance();
    mgr.define("falseFlag", true);

    std::string envName = "METRO_FF_FALSEFLAG";
#ifdef _WIN32
    _putenv_s(envName.c_str(), "0");
#else
    setenv(envName.c_str(), "0", 1);
#endif
    mgr.refresh();
    assert(mgr.isEnabled("falseFlag") == false);

#ifdef _WIN32
    _putenv_s(envName.c_str(), "");
#else
    unsetenv(envName.c_str());
#endif
    mgr.resetAll();

    PASS();
}

void test_manager_env_var_no_override()
{
    TEST("FeatureFlagManager env var does not affect unset flags");

    auto& mgr = FeatureFlagManager::instance();
    mgr.define("unsetFlag", true);

    std::string envName = "METRO_FF_UNSETFLAG";
#ifdef _WIN32
    _putenv_s(envName.c_str(), "false");
#else
    setenv(envName.c_str(), "false", 1);
#endif
    mgr.refresh();
    assert(mgr.isEnabled("unsetFlag") == false);

#ifdef _WIN32
    _putenv_s(envName.c_str(), "");
#else
    unsetenv(envName.c_str());
#endif
    mgr.resetAll();

    PASS();
}

int main()
{
    setup();

    std::printf("ofx-core-test: OpenFX Bootstrap + Feature Flag Unit Tests\n");
    std::printf("==========================================================\n\n");

    test_host_info_defaults();
    test_host_null_handle();
    test_param_type_strings();
    test_param_spec_defaults();
    test_ofx_core_constants();
    test_plugin_identifiers();
    test_image_effect_contexts();

    std::printf("\n--- Feature Flag Tests ---\n\n");

    test_feature_flag_defaults();
    test_feature_flag_enable_disable();
    test_feature_flag_rollout_clamping();
    test_feature_flag_rollout_checks();
    test_feature_flag_value_ctor();
    test_manager_singleton();
    test_manager_define_and_query();
    test_manager_set_enabled();
    test_manager_reset_all();
    test_manager_unknown_flag();
    test_manager_remote_override();
    test_manager_env_var_override();
    test_manager_env_var_false_values();
    test_manager_env_var_no_override();
    test_manager_toml_config();
    test_manager_toml_config_missing_file();
    test_manager_toml_config_with_comments();
    test_manager_toml_config_skip_other_sections();
    test_feature_flag_thread_safe();

    teardown();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);

    return (s_passCount == s_testCount) ? 0 : 1;
}
