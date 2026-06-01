#include <cstdio>
#include <cassert>
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

#define FAIL(msg) do { \
    std::printf("FAIL: %s\n", msg); \
} while(0)

static const char *kCharsetDefault = " .:-=+*#%@";
static const int   kCharsetLen = 10;

static float luminance(float r, float g, float b)
{
    return 0.299f * r + 0.587f * g + 0.114f * b;
}

static char mapLuminance(float luma, const char *charset, int charsetLen)
{
    float clamped = luma < 0.0f ? 0.0f : (luma > 1.0f ? 1.0f : luma);
    int idx = static_cast<int>(clamped * (charsetLen - 1) + 0.5f);
    if (idx < 0) idx = 0;
    if (idx >= charsetLen) idx = charsetLen - 1;
    return charset[idx];
}

void test_luminance_standard()
{
    TEST("luminance of black is 0.0");
    assert(std::abs(luminance(0.0f, 0.0f, 0.0f)) < 0.001f);
    PASS();

    TEST("luminance of white is 1.0");
    assert(std::abs(luminance(1.0f, 1.0f, 1.0f) - 1.0f) < 0.001f);
    PASS();

    TEST("luminance of neutral gray is 0.5");
    assert(std::abs(luminance(0.5f, 0.5f, 0.5f) - 0.5f) < 0.001f);
    PASS();

    TEST("luminance weights green more than red");
    float l1 = luminance(1.0f, 0.0f, 0.0f);
    float l2 = luminance(0.0f, 1.0f, 0.0f);
    assert(l2 > l1);
    PASS();
}

void test_luminance_pure_colors()
{
    TEST("luminance of pure red");
    float l = luminance(1.0f, 0.0f, 0.0f);
    assert(std::abs(l - 0.299f) < 0.001f);
    PASS();

    TEST("luminance of pure green");
    l = luminance(0.0f, 1.0f, 0.0f);
    assert(std::abs(l - 0.587f) < 0.001f);
    PASS();

    TEST("luminance of pure blue");
    l = luminance(0.0f, 0.0f, 1.0f);
    assert(std::abs(l - 0.114f) < 0.001f);
    PASS();
}

void test_map_luminance_bounds()
{
    TEST("mapLuminance 0.0 returns first char");
    assert(mapLuminance(0.0f, kCharsetDefault, kCharsetLen) == ' ');
    PASS();

    TEST("mapLuminance 1.0 returns last char");
    assert(mapLuminance(1.0f, kCharsetDefault, kCharsetLen) == '@');
    PASS();

    TEST("mapLuminance <0 clamps to first char");
    assert(mapLuminance(-1.0f, kCharsetDefault, kCharsetLen) == ' ');
    PASS();

    TEST("mapLuminance >1 clamps to last char");
    assert(mapLuminance(2.0f, kCharsetDefault, kCharsetLen) == '@');
    PASS();
}

void test_map_luminance_distribution()
{
    TEST("mapLuminance 0.5 returns middle char");
    {
        char c = mapLuminance(0.5f, kCharsetDefault, kCharsetLen);
        assert(c == '=' || c == '+');
    }
    PASS();

    TEST("mapLuminance returns distinct chars for distinct inputs");
    {
        char c1 = mapLuminance(0.05f, kCharsetDefault, kCharsetLen);
        char c2 = mapLuminance(0.95f, kCharsetDefault, kCharsetLen);
        assert(c1 != c2);
    }
    PASS();

    TEST("mapLuminance first non-space at ~0.1");
    {
        char c = mapLuminance(0.1f, kCharsetDefault, kCharsetLen);
        assert(c != ' ');
    }
    PASS();
}

void test_charset_defines()
{
    TEST("charset has correct length");
    assert(kCharsetLen == 10);
    PASS();

    TEST("charset starts with space and ends with @");
    assert(kCharsetDefault[0] == ' ');
    assert(kCharsetDefault[kCharsetLen - 1] == '@');
    PASS();
}

int main()
{
    std::printf("metro-ascii-test: ASCII Algorithm Unit Tests\n");
    std::printf("============================================\n\n");

    test_luminance_standard();
    test_luminance_pure_colors();
    test_map_luminance_bounds();
    test_map_luminance_distribution();
    test_charset_defines();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);
    return (s_passCount == s_testCount) ? 0 : 1;
}
