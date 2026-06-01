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

static bool isIdentityCheck(double rX, double rY, double gX, double gY, double bX, double bY)
{
    return rX == 0.0 && rY == 0.0 && gX == 0.0 && gY == 0.0 && bX == 0.0 && bY == 0.0;
}

void test_identity_all_zero()
{
    TEST("isIdentity returns true when all shifts are zero");
    assert(isIdentityCheck(0.0, 0.0, 0.0, 0.0, 0.0, 0.0) == true);
    PASS();
}

void test_identity_red_shifted()
{
    TEST("isIdentity returns false when red is shifted");
    assert(isIdentityCheck(1.0, 0.0, 0.0, 0.0, 0.0, 0.0) == false);
    PASS();
}

void test_identity_green_shifted()
{
    TEST("isIdentity returns false when green is shifted");
    assert(isIdentityCheck(0.0, 0.0, 2.0, 0.0, 0.0, 0.0) == false);
    PASS();
}

void test_identity_blue_shifted()
{
    TEST("isIdentity returns false when blue is shifted");
    assert(isIdentityCheck(0.0, 0.0, 0.0, 0.0, 3.0, 0.0) == false);
    PASS();
}

void test_identity_all_negative()
{
    TEST("isIdentity returns false when shifts are non-zero, all negative");
    assert(isIdentityCheck(-1.0, -2.0, -3.0, -4.0, -5.0, -6.0) == false);
    PASS();
}

void test_identity_mixed_signs()
{
    TEST("isIdentity returns false when shifts have mixed signs");
    assert(isIdentityCheck(1.0, -1.0, 2.0, -2.0, 3.0, -3.0) == false);
    PASS();
}

void test_param_type_strings()
{
    TEST("chromab param type strings are valid");
    assert(std::strcmp(typeString(Type::Double), kOfxParamTypeDouble) == 0);
    assert(std::strcmp(typeString(Type::Double2D), kOfxParamTypeDouble2D) == 0);
    PASS();
}

void test_param_spec_defaults()
{
    TEST("chromab uses correct ParamSpec defaults");
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
    std::printf("chromab-test: Chromatic Aberration Plugin Unit Tests\n");
    std::printf("===================================================\n\n");

    test_identity_all_zero();
    test_identity_red_shifted();
    test_identity_green_shifted();
    test_identity_blue_shifted();
    test_identity_all_negative();
    test_identity_mixed_signs();
    test_param_type_strings();
    test_param_spec_defaults();
    test_render_window_constants();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);

    return (s_passCount == s_testCount) ? 0 : 1;
}
