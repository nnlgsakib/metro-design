#include <cstdio>
#include <cassert>
#include <cstring>

#include <ofxCore.h>
#include <ofxProperty.h>
#include <ofxParam.h>
#include <ofxImageEffect.h>

#include "metro/ofx/Host.hpp"
#include "metro/ofx/Plugin.hpp"
#include "metro/ofx/param/ParamManager.hpp"

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

int main()
{
    std::printf("ofx-core-test: OpenFX Bootstrap Unit Tests\n");
    std::printf("==========================================\n\n");

    test_host_info_defaults();
    test_host_null_handle();
    test_param_type_strings();
    test_param_spec_defaults();
    test_ofx_core_constants();
    test_plugin_identifiers();
    test_image_effect_contexts();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);

    return (s_passCount == s_testCount) ? 0 : 1;
}
