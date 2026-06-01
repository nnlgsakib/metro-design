// Copyright (c) 2026 Metro Design. All rights reserved.
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cmath>
#include <cfloat>

#include <ofxCore.h>
#include <ofxImageEffect.h>

#include "metro/ofx/param/ParamManager.hpp"

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

static float gaussian(float x, float sigma)
{
    return std::exp(-(x * x) / (2.0f * sigma * sigma));
}

static void hsvToRgb(float h, float s, float v, float &r, float &g, float &b)
{
    float c = v * s;
    float hp = h / 60.0f;
    float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    float m = v - c;
    int hi = static_cast<int>(hp) % 6;
    switch (hi) {
        case 0: r = c; g = x; b = 0.0f; break;
        case 1: r = x; g = c; b = 0.0f; break;
        case 2: r = 0.0f; g = c; b = x; break;
        case 3: r = 0.0f; g = x; b = c; break;
        case 4: r = x; g = 0.0f; b = c; break;
        case 5: r = c; g = 0.0f; b = x; break;
        default: r = 0.0f; g = 0.0f; b = 0.0f; break;
    }
    r += m; g += m; b += m;
}

static float luminance(const float *pixel)
{
    return 0.2126f * pixel[0] + 0.7152f * pixel[1] + 0.0722f * pixel[2];
}

static bool isIdentityCheck(double brightness)
{
    return brightness == 0.0;
}

void test_identity_brightness_zero()
{
    TEST("isIdentity returns true when brightness is 0");
    assert(isIdentityCheck(0.0) == true);
    PASS();
}

void test_identity_brightness_nonzero()
{
    TEST("isIdentity returns false when brightness > 0");
    assert(isIdentityCheck(1.0) == false);
    assert(isIdentityCheck(0.5) == false);
    assert(isIdentityCheck(5.0) == false);
    PASS();
}

void test_gaussian_center()
{
    TEST("gaussian at center (x=0) returns 1.0");
    float v = gaussian(0.0f, 1.0f);
    assert(std::fabs(v - 1.0f) < 1e-6f);
    PASS();
}

void test_gaussian_far()
{
    TEST("gaussian far from center returns near 0");
    float v = gaussian(100.0f, 1.0f);
    assert(v < 1e-6f);
    PASS();
}

void test_hsv_to_rgb_red()
{
    TEST("HSV(0,1,1) converts to RGB(1,0,0)");
    float r, g, b;
    hsvToRgb(0.0f, 1.0f, 1.0f, r, g, b);
    assert(std::fabs(r - 1.0f) < 1e-4f);
    assert(std::fabs(g) < 1e-4f);
    assert(std::fabs(b) < 1e-4f);
    PASS();
}

void test_hsv_to_rgb_cyan()
{
    TEST("HSV(180,1,1) converts to RGB(0,1,1)");
    float r, g, b;
    hsvToRgb(180.0f, 1.0f, 1.0f, r, g, b);
    assert(std::fabs(r) < 1e-4f);
    assert(std::fabs(g - 1.0f) < 1e-4f);
    assert(std::fabs(b - 1.0f) < 1e-4f);
    PASS();
}

void test_hsv_to_rgb_black()
{
    TEST("HSV(0,1,0) converts to RGB(0,0,0)");
    float r, g, b;
    hsvToRgb(0.0f, 1.0f, 0.0f, r, g, b);
    assert(std::fabs(r) < 1e-4f);
    assert(std::fabs(g) < 1e-4f);
    assert(std::fabs(b) < 1e-4f);
    PASS();
}

void test_luminance_white()
{
    TEST("luminance of white pixel is 1.0");
    float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float l = luminance(white);
    assert(std::fabs(l - 1.0f) < 1e-4f);
    PASS();
}

void test_luminance_black()
{
    TEST("luminance of black pixel is 0.0");
    float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float l = luminance(black);
    assert(std::fabs(l) < 1e-4f);
    PASS();
}

void test_luminance_gray()
{
    TEST("luminance of gray pixel uses Rec.709 weights");
    float gray[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    float l = luminance(gray);
    assert(std::fabs(l - 0.5f) < 1e-4f);
    PASS();
}

void test_param_type_strings()
{
    TEST("lensflare param type strings are valid");
    assert(std::strcmp(typeString(Type::Double), kOfxParamTypeDouble) == 0);
    assert(std::strcmp(typeString(Type::Integer), kOfxParamTypeInteger) == 0);
    PASS();
}

void test_param_spec_defaults()
{
    TEST("lensflare uses correct ParamSpec defaults");
    ParamSpec spec;
    assert(spec.type == Type::Double);
    assert(spec.doubleDefault == 0.5);
    assert(spec.intDefault == 0);
    assert(spec.booleanDefault == false);
    PASS();
}

void test_render_window_constants()
{
    TEST("OFX render window constants are correct");
    assert(kOfxStatOK == 0);
    assert(kOfxStatReplyDefault == 2);
    assert(kOfxStatErrBadHandle == -9);
    PASS();
}

int main()
{
    std::printf("lensflare-test: Lens Flare Plugin Unit Tests\n");
    std::printf("============================================\n\n");

    test_identity_brightness_zero();
    test_identity_brightness_nonzero();
    test_gaussian_center();
    test_gaussian_far();
    test_hsv_to_rgb_red();
    test_hsv_to_rgb_cyan();
    test_hsv_to_rgb_black();
    test_luminance_white();
    test_luminance_black();
    test_luminance_gray();
    test_param_type_strings();
    test_param_spec_defaults();
    test_render_window_constants();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);

    return (s_passCount == s_testCount) ? 0 : 1;
}
