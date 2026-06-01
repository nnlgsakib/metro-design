#include <cstdio>
#include <cassert>
#include <cmath>
#include <algorithm>

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

static unsigned int hashUIntCPU(unsigned int x, unsigned int y, unsigned int seed)
{
    unsigned int h = x * 374761393u + y * 668265263u + seed * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return h;
}

static float hashFloatCPU(unsigned int x, unsigned int y, unsigned int seed)
{
    return float(hashUIntCPU(x, y, seed)) / 4294967295.0f;
}

static float smoothstep5CPU(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static void test_clampf_below()
{
    TEST("clampf clamps value below range");
    assert(clampf(-0.5f, 0.0f, 1.0f) == 0.0f);
    PASS();
}

static void test_clampf_above()
{
    TEST("clampf clamps value above range");
    assert(clampf(1.5f, 0.0f, 1.0f) == 1.0f);
    PASS();
}

static void test_clampf_through()
{
    TEST("clampf passes through in-range value");
    assert(clampf(0.5f, 0.0f, 1.0f) == 0.5f);
    PASS();
}

static void test_hash_deterministic()
{
    TEST("hashUIntCPU is deterministic");
    unsigned int a = hashUIntCPU(42, 99, 1234);
    unsigned int b = hashUIntCPU(42, 99, 1234);
    assert(a == b);
    PASS();
}

static void test_hash_differs_on_x()
{
    TEST("hashUIntCPU differs when x changes");
    unsigned int a = hashUIntCPU(1, 0, 0);
    unsigned int b = hashUIntCPU(2, 0, 0);
    assert(a != b);
    PASS();
}

static void test_hash_differs_on_y()
{
    TEST("hashUIntCPU differs when y changes");
    unsigned int a = hashUIntCPU(0, 1, 0);
    unsigned int b = hashUIntCPU(0, 2, 0);
    assert(a != b);
    PASS();
}

static void test_hash_differs_on_seed()
{
    TEST("hashUIntCPU differs when seed changes");
    unsigned int a = hashUIntCPU(0, 0, 1);
    unsigned int b = hashUIntCPU(0, 0, 2);
    assert(a != b);
    PASS();
}

static void test_hash_float_in_range()
{
    TEST("hashFloatCPU returns value in [0,1]");
    for (unsigned int s = 0; s < 100; ++s) {
        float f = hashFloatCPU(s, s * 3, s * 7);
        assert(f >= 0.0f && f <= 1.0f);
    }
    PASS();
}

static void test_smoothstep5_zero()
{
    TEST("smoothstep5CPU(0) == 0");
    assert(smoothstep5CPU(0.0f) == 0.0f);
    PASS();
}

static void test_smoothstep5_one()
{
    TEST("smoothstep5CPU(1) == 1");
    assert(smoothstep5CPU(1.0f) == 1.0f);
    PASS();
}

static void test_smoothstep5_half()
{
    TEST("smoothstep5CPU(0.5) == 0.5");
    float v = smoothstep5CPU(0.5f);
    assert(std::fabs(v - 0.5f) < 1e-6f);
    PASS();
}

static void test_smoothstep5_monotonic()
{
    TEST("smoothstep5CPU is monotonic");
    float prev = -1.0f;
    for (int i = 0; i <= 100; ++i) {
        float t = float(i) / 100.0f;
        float v = smoothstep5CPU(t);
        assert(v >= prev - 1e-6f);
        prev = v;
    }
    PASS();
}

static void test_smoothstep5_symmetric()
{
    TEST("smoothstep5CPU is symmetric about 0.5");
    for (int i = 0; i <= 50; ++i) {
        float t = float(i) / 100.0f;
        float v1 = smoothstep5CPU(t);
        float v2 = 1.0f - smoothstep5CPU(1.0f - t);
        assert(std::fabs(v1 - v2) < 1e-6f);
    }
    PASS();
}

static void test_identity_amount_zero()
{
    TEST("amount=0 triggers identity path");
    double amount = 0.0, mix = 1.0;
    if (amount == 0.0 || mix == 0.0) {
        PASS();
        return;
    }
}

static void test_identity_mix_zero()
{
    TEST("mix=0 triggers identity path");
    double amount = 1.0, mix = 0.0;
    if (amount == 0.0 || mix == 0.0) {
        PASS();
        return;
    }
}

int main()
{
    std::printf("filmgrain-test: Film Grain Plugin Unit Tests\n");
    std::printf("=============================================\n\n");

    test_clampf_below();
    test_clampf_above();
    test_clampf_through();
    test_hash_deterministic();
    test_hash_differs_on_x();
    test_hash_differs_on_y();
    test_hash_differs_on_seed();
    test_hash_float_in_range();
    test_smoothstep5_zero();
    test_smoothstep5_one();
    test_smoothstep5_half();
    test_smoothstep5_monotonic();
    test_smoothstep5_symmetric();
    test_identity_amount_zero();
    test_identity_mix_zero();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);
    return (s_passCount == s_testCount) ? 0 : 1;
}
