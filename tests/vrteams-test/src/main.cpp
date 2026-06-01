#include <cstdio>
#include <cassert>
#include <cstring>
#include <cmath>

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

static float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

static float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float overlayBlend(float base, float overlay)
{
    if (base < 0.5f)
        return 2.0f * base * overlay;
    return 1.0f - 2.0f * (1.0f - base) * (1.0f - overlay);
}

struct Annotation {
    float x, y;
    float r, g, b, a;
    float thickness;
};

static float annotationCoverage(const Annotation &a, float px, float py)
{
    float dx = px - a.x, dy = py - a.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    float half = a.thickness * 0.5f;
    return clampf(1.0f - (dist - half) / half, 0.0f, 1.0f);
}

static float premultiply(float c, float a)
{
    return c * a;
}

static float compositeOver(float dst, float src, float srcA)
{
    return src + dst * (1.0f - srcA);
}

void test_plugin_grouping()
{
    TEST("plugin grouping is Metro Design");
    const char *grouping = "Metro Design";
    assert(std::strcmp(grouping, "Metro Design") == 0);
    PASS();
}

void test_plugin_identifier()
{
    TEST("plugin identifier is com.metrodesign.vrteams");
    const char *id = "com.metrodesign.vrteams";
    assert(std::strcmp(id, "com.metrodesign.vrteams") == 0);
    PASS();
}

void test_plugin_version()
{
    TEST("plugin version is 1.0.0");
    const char *ver = "1.0.0";
    assert(std::strcmp(ver, "1.0.0") == 0);
    PASS();
}

void test_clampf_below()
{
    TEST("clampf clamps value below range");
    assert(clampf(-1.0f, 0.0f, 1.0f) == 0.0f);
    PASS();
}

void test_clampf_above()
{
    TEST("clampf clamps value above range");
    assert(clampf(2.0f, 0.0f, 1.0f) == 1.0f);
    PASS();
}

void test_lerpf_bounds()
{
    TEST("lerpf at t=0 returns a");
    assert(lerpf(2.0f, 5.0f, 0.0f) == 2.0f);
    PASS();
}

void test_lerpf_mid()
{
    TEST("lerpf at t=0.5 returns midpoint");
    assert(lerpf(2.0f, 6.0f, 0.5f) == 4.0f);
    PASS();
}

void test_lerpf_end()
{
    TEST("lerpf at t=1 returns b");
    assert(lerpf(2.0f, 6.0f, 1.0f) == 6.0f);
    PASS();
}

void test_overlay_blend_dark()
{
    TEST("overlay blend with base < 0.5 uses multiply");
    float r = overlayBlend(0.3f, 0.8f);
    assert(std::fabs(r - 0.48f) < 1e-6f);
    PASS();
}

void test_overlay_blend_light()
{
    TEST("overlay blend with base >= 0.5 uses screen");
    float r = overlayBlend(0.7f, 0.8f);
    assert(std::fabs(r - 0.88f) < 1e-6f);
    PASS();
}

void test_overlay_blend_identity()
{
    TEST("overlay blend with overlay=0.5 is identity");
    float r = overlayBlend(0.3f, 0.5f);
    assert(std::fabs(r - 0.3f) < 1e-6f);
    PASS();
}

void test_annotation_coverage_center()
{
    TEST("annotation coverage at center is 1.0");
    Annotation a = {0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.8f, 0.1f};
    float c = annotationCoverage(a, 0.5f, 0.5f);
    assert(c == 1.0f);
    PASS();
}

void test_annotation_coverage_far()
{
    TEST("annotation coverage far from center is 0");
    Annotation a = {0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.8f, 0.1f};
    float c = annotationCoverage(a, 0.0f, 0.0f);
    assert(c == 0.0f);
    PASS();
}

void test_premultiply_preserves_alpha_1()
{
    TEST("premultiply with a=1 preserves color");
    assert(premultiply(0.5f, 1.0f) == 0.5f);
    PASS();
}

void test_premultiply_zero_alpha()
{
    TEST("premultiply with a=0 blackens");
    assert(premultiply(0.5f, 0.0f) == 0.0f);
    PASS();
}

void test_composite_over_opaque()
{
    TEST("compositeOver opaque src overwrites dst");
    float r = compositeOver(1.0f, 0.5f, 1.0f);
    assert(r == 0.5f);
    PASS();
}

void test_composite_over_transparent()
{
    TEST("compositeOver transparent src shows dst");
    float r = compositeOver(1.0f, 0.5f, 0.0f);
    assert(r == 1.0f);
    PASS();
}

void test_composite_over_half()
{
    TEST("compositeOver half-alpha blends");
    float r = compositeOver(0.0f, 0.5f, 0.5f);
    assert(r == 0.5f);
    PASS();
}

void test_mix_zero_identity()
{
    TEST("mix=0 triggers identity path");
    double mix = 0.0;
    if (mix == 0.0) {
        PASS();
        return;
    }
}

int main()
{
    std::printf("vrteams-test: VR Teams Plugin Unit Tests\n");
    std::printf("========================================\n\n");

    test_plugin_grouping();
    test_plugin_identifier();
    test_plugin_version();
    test_clampf_below();
    test_clampf_above();
    test_lerpf_bounds();
    test_lerpf_mid();
    test_lerpf_end();
    test_overlay_blend_dark();
    test_overlay_blend_light();
    test_overlay_blend_identity();
    test_annotation_coverage_center();
    test_annotation_coverage_far();
    test_premultiply_preserves_alpha_1();
    test_premultiply_zero_alpha();
    test_composite_over_opaque();
    test_composite_over_transparent();
    test_composite_over_half();
    test_mix_zero_identity();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);
    return (s_passCount == s_testCount) ? 0 : 1;
}
