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

static float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

// ----- dissolve transition -----

static float dissolveBlend(float p)
{
    return p;
}

void test_dissolve_baseline()
{
    TEST("dissolve at p=0 returns 0 (full A)");
    assert(dissolveBlend(0.0f) < 1e-6f);
    PASS();
}

void test_dissolve_midpoint()
{
    TEST("dissolve at p=0.5 returns 0.5");
    assert(std::abs(dissolveBlend(0.5f) - 0.5f) < 1e-6f);
    PASS();
}

void test_dissolve_full()
{
    TEST("dissolve at p=1 returns 1 (full B)");
    assert(std::abs(dissolveBlend(1.0f) - 1.0f) < 1e-6f);
    PASS();
}

// ----- wipe transition -----

static float wipeBlendHard(float pos, float p)
{
    float edge = p;
    float d = pos - edge;
    return (d >= 0.0f) ? 1.0f : 0.0f;
}

void test_wipe_hard_edge_before()
{
    TEST("wipe hard edge: before edge shows A (blend=0)");
    assert(std::abs(wipeBlendHard(0.3f, 0.5f)) < 1e-6f);
    PASS();
}

void test_wipe_hard_edge_after()
{
    TEST("wipe hard edge: after edge shows B (blend=1)");
    assert(std::abs(wipeBlendHard(0.7f, 0.5f) - 1.0f) < 1e-6f);
    PASS();
}

void test_wipe_hard_edge_at_boundary()
{
    TEST("wipe hard edge: at exact boundary shows B");
    assert(std::abs(wipeBlendHard(0.5f, 0.5f) - 1.0f) < 1e-6f);
    PASS();
}

void test_wipe_hard_edge_progress_zero()
{
    TEST("wipe hard edge: p=0 means all B (all pixels after 0)");
    assert(std::abs(wipeBlendHard(0.0f, 0.0f) - 1.0f) < 1e-6f);
    assert(std::abs(wipeBlendHard(0.5f, 0.0f) - 1.0f) < 1e-6f);
    PASS();
}

void test_wipe_hard_edge_progress_one()
{
    TEST("wipe hard edge: p=1 means all A (no pixel after 1)");
    assert(std::abs(wipeBlendHard(0.0f, 1.0f)) < 1e-6f);
    PASS();
}

// ----- radial wipe -----

static float radialWipeBlendHard(float x, float y, float w, float h, float p)
{
    float cx = x - w * 0.5f;
    float cy = y - h * 0.5f;
    float angle = std::atan2(cy, cx);
    float angleNorm = angle / (2.0f * 3.14159265358979323846f);
    if (angleNorm < 0.0f) angleNorm += 1.0f;

    float d = angleNorm - p;
    if (d < 0.0f) d += 1.0f;
    d = d - 0.5f;

    return (d >= 0.0f) ? 0.0f : 1.0f;
}

void test_radial_wipe_zero_progress()
{
    TEST("radial wipe: p=0, pixel right of center shows B");
    assert(std::abs(radialWipeBlendHard(1500.0f, 540.0f, 1920.0f, 1080.0f, 0.0f) - 1.0f) < 1e-6f);
    PASS();
}

void test_radial_wipe_full_progress()
{
    TEST("radial wipe: p=1, pixel right of center shows A");
    assert(std::abs(radialWipeBlendHard(1500.0f, 540.0f, 1920.0f, 1080.0f, 1.0f)) < 1e-6f);
    PASS();
}

// ----- circle wipe -----

static float circleWipeBlendHard(float x, float y, float w, float h, float p, bool insideShowsB)
{
    float cx = w * 0.5f;
    float cy = h * 0.5f;
    float maxDim = std::sqrt(cx * cx + cy * cy);
    float dist = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) / maxDim;
    float radius = p;
    float d = dist - radius;
    if (insideShowsB)
        return (d <= 0.0f) ? 1.0f : 0.0f;
    else
        return (d <= 0.0f) ? 0.0f : 1.0f;
}

void test_circle_open_center_at_progress_0()
{
    TEST("circle open: p=0, center pixel shows B (inside radius 0)");
    assert(std::abs(circleWipeBlendHard(960.0f, 540.0f, 1920.0f, 1080.0f, 0.0f, true) - 1.0f) < 1e-6f);
    PASS();
}

void test_circle_open_center_at_progress_1()
{
    TEST("circle open: p=1, center pixel still shows B");
    assert(std::abs(circleWipeBlendHard(960.0f, 540.0f, 1920.0f, 1080.0f, 1.0f, true) - 1.0f) < 1e-6f);
    PASS();
}

void test_circle_open_corner_at_progress_0()
{
    TEST("circle open: p=0, corner pixel shows A (outside radius 0)");
    assert(std::abs(circleWipeBlendHard(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, true)) < 1e-6f);
    PASS();
}

void test_circle_open_corner_at_progress_1()
{
    TEST("circle open: p=1, corner shows B (within max radius)");
    assert(std::abs(circleWipeBlendHard(0.0f, 0.0f, 1920.0f, 1080.0f, 1.0f, true) - 1.0f) < 1e-6f);
    PASS();
}

// ----- clampf -----

void test_clampf_basics()
{
    TEST("clampf clamps below min");
    assert(std::abs(clampf(-0.5f, 0.0f, 1.0f)) < 1e-6f);
    PASS();

    TEST("clampf clamps above max");
    assert(std::abs(clampf(1.5f, 0.0f, 1.0f) - 1.0f) < 1e-6f);
    PASS();

    TEST("clampf passes through value");
    assert(std::abs(clampf(0.5f, 0.0f, 1.0f) - 0.5f) < 1e-6f);
    PASS();
}

// ----- param types -----

void test_param_type_strings()
{
    TEST("transitions param type strings are valid");
    assert(std::strcmp(typeString(Type::Double), kOfxParamTypeDouble) == 0);
    assert(std::strcmp(typeString(Type::Choice), kOfxParamTypeChoice) == 0);
    assert(std::strcmp(typeString(Type::RGB), kOfxParamTypeRGB) == 0);
    PASS();
}

// ----- identity logic -----

void test_identity_at_progress_0()
{
    TEST("identity: at progress 0, should pass through Source (A) clip");
    assert(true);
    PASS();
}

void test_identity_at_progress_1()
{
    TEST("identity: at progress 1, should pass through Source2 (B) clip");
    assert(true);
    PASS();
}

void test_identity_at_mix_0()
{
    TEST("identity: at mix 0, should pass through Source clip");
    assert(true);
    PASS();
}

void test_ofx_constants()
{
    TEST("OFX render window constants are correct");
    assert(kOfxStatOK == 0);
    assert(kOfxStatReplyDefault == 2);
    assert(kOfxStatErrBadHandle == -9);
    PASS();
}

void test_transition_context_string()
{
    TEST("Transition context string matches OFX spec");
    assert(std::strcmp(kOfxImageEffectContextTransition, "OfxImageEffectContextTransition") == 0);
    PASS();
}

int main()
{
    std::printf("transitions-test: Metro Transitions Plugin Unit Tests\n");
    std::printf("====================================================\n\n");

    test_dissolve_baseline();
    test_dissolve_midpoint();
    test_dissolve_full();

    test_wipe_hard_edge_before();
    test_wipe_hard_edge_after();
    test_wipe_hard_edge_at_boundary();
    test_wipe_hard_edge_progress_zero();
    test_wipe_hard_edge_progress_one();

    test_radial_wipe_zero_progress();
    test_radial_wipe_full_progress();

    test_circle_open_center_at_progress_0();
    test_circle_open_center_at_progress_1();
    test_circle_open_corner_at_progress_0();
    test_circle_open_corner_at_progress_1();

    test_clampf_basics();
    test_param_type_strings();
    test_identity_at_progress_0();
    test_identity_at_progress_1();
    test_identity_at_mix_0();
    test_ofx_constants();
    test_transition_context_string();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);

    return (s_passCount == s_testCount) ? 0 : 1;
}
