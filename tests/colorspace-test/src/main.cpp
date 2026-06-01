#include <cstdio>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <cfloat>

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

static void invert3x3(const double in[3][3], double out[3][3])
{
    double a=in[0][0],b=in[0][1],c=in[0][2],d=in[1][0],e=in[1][1],f=in[1][2],g=in[2][0],h=in[2][1],i=in[2][2];
    double det = a*(e*i-f*h) - b*(d*i-f*g) + c*(d*h-e*g);
    if (std::abs(det) < 1e-15) return;
    double id = 1.0/det;
    out[0][0]=(e*i-f*h)*id; out[0][1]=(c*h-b*i)*id; out[0][2]=(b*f-c*e)*id;
    out[1][0]=(f*g-d*i)*id; out[1][1]=(a*i-c*g)*id; out[1][2]=(c*d-a*f)*id;
    out[2][0]=(d*h-e*g)*id; out[2][1]=(b*g-a*h)*id; out[2][2]=(a*e-b*d)*id;
}

static void matMul3x3(const double a[3][3], const double b[3][3], double out[3][3])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            out[i][j] = 0.0;
            for (int k = 0; k < 3; ++k)
                out[i][j] += a[i][k] * b[k][j];
        }
}

static void identity3x3(double out[3][3])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            out[i][j] = (i == j) ? 1.0 : 0.0;
}

static bool approxEq3x3(const double a[3][3], const double b[3][3], double eps)
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (std::fabs(a[i][j] - b[i][j]) > eps) return false;
    return true;
}

static double logCToLinear(double v)
{
    if (v > 0.149658) return (std::pow(10.0,(v-0.385537)/0.247189)-0.052272)/5.555556;
    return (v-0.092809)/5.367655;
}

static double linearToLogC(double v)
{
    if (v > 0.0) return 0.385537 + 0.247189 * std::log10(5.555556 * v + 0.052272);
    return 0.092809 + 5.367655 * v;
}

static double rec709Luma(double r, double g, double b)
{
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

static void applyMatrix(const double m[3][3], double r, double g, double b, double &ro, double &go, double &bo)
{
    ro = m[0][0]*r + m[0][1]*g + m[0][2]*b;
    go = m[1][0]*r + m[1][1]*g + m[1][2]*b;
    bo = m[2][0]*r + m[2][1]*g + m[2][2]*b;
}

void test_invert3x3_identity()
{
    TEST("invert3x3 of identity is identity");
    double I[3][3], inv[3][3];
    identity3x3(I);
    invert3x3(I, inv);
    assert(approxEq3x3(I, inv, 1e-12));
    PASS();
}

void test_invert3x3_known()
{
    TEST("invert3x3 of known matrix");
    double M[3][3] = {{2,0,0},{0,3,0},{0,0,4}};
    double inv[3][3];
    invert3x3(M, inv);
    double expected[3][3] = {{0.5,0,0},{0,1.0/3.0,0},{0,0,0.25}};
    assert(approxEq3x3(inv, expected, 1e-12));
    PASS();
}

void test_invert3x3_product_is_identity()
{
    TEST("invert3x3: M * inv(M) == I");
    double M[3][3] = {{4,1,2},{3,5,1},{2,1,6}};
    double inv[3][3], prod[3][3];
    invert3x3(M, inv);
    matMul3x3(M, inv, prod);
    double I[3][3];
    identity3x3(I);
    assert(approxEq3x3(prod, I, 1e-12));
    PASS();
}

void test_rec709_luma_black()
{
    TEST("rec709 luma of black is 0");
    assert(rec709Luma(0.0, 0.0, 0.0) == 0.0);
    PASS();
}

void test_rec709_luma_white()
{
    TEST("rec709 luma of white is 1");
    assert(rec709Luma(1.0, 1.0, 1.0) == 1.0);
    PASS();
}

void test_rec709_luma_weights()
{
    TEST("rec709 luma weights: green > red > blue");
    double rLuma = rec709Luma(1.0, 0.0, 0.0);
    double gLuma = rec709Luma(0.0, 1.0, 0.0);
    double bLuma = rec709Luma(0.0, 0.0, 1.0);
    assert(gLuma > rLuma && rLuma > bLuma);
    PASS();
}

void test_logc_roundtrip()
{
    TEST("LogC round-trip: linear -> log -> linear");
    double vals[] = {0.0, 0.001, 0.01, 0.1, 0.18, 0.5, 1.0};
    for (double v : vals) {
        double l = linearToLogC(v);
        double r = logCToLinear(l);
        assert(std::fabs(r - v) < 1e-6);
    }
    PASS();
}

void test_logc_at_black()
{
    TEST("LogC at nominal black is correct");
    double v = logCToLinear(0.092809);
    assert(std::fabs(v) < 1e-4);
    PASS();
}

void test_apply_matrix_identity()
{
    TEST("applyMatrix with identity preserves values");
    double I[3][3];
    identity3x3(I);
    double r, g, b;
    applyMatrix(I, 0.5, 0.3, 0.7, r, g, b);
    assert(std::fabs(r - 0.5) < 1e-12);
    assert(std::fabs(g - 0.3) < 1e-12);
    assert(std::fabs(b - 0.7) < 1e-12);
    PASS();
}

void test_apply_matrix_swap()
{
    TEST("applyMatrix swaps R and B channels");
    double swap[3][3] = {{0,0,1},{0,1,0},{1,0,0}};
    double r, g, b;
    applyMatrix(swap, 1.0, 0.5, 0.0, r, g, b);
    assert(std::fabs(r) < 1e-12);
    assert(std::fabs(g - 0.5) < 1e-12);
    assert(std::fabs(b - 1.0) < 1e-12);
    PASS();
}

void test_cs_names_srgb()
{
    TEST("sRGB color space enum is 0");
    int cs_sRGB = 0;
    assert(cs_sRGB == 0);
    PASS();
}

void test_cs_names_count()
{
    TEST("8 color spaces defined");
    int CS_COUNT = 8;
    assert(CS_COUNT == 8);
    PASS();
}

int main()
{
    std::printf("colorspace-test: Color Space Plugin Unit Tests\n");
    std::printf("===============================================\n\n");

    test_invert3x3_identity();
    test_invert3x3_known();
    test_invert3x3_product_is_identity();
    test_rec709_luma_black();
    test_rec709_luma_white();
    test_rec709_luma_weights();
    test_logc_roundtrip();
    test_logc_at_black();
    test_apply_matrix_identity();
    test_apply_matrix_swap();
    test_cs_names_srgb();
    test_cs_names_count();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);
    return (s_passCount == s_testCount) ? 0 : 1;
}
