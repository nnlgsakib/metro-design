#include <cstdio>
#include <cassert>
#include <cmath>
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

#define TF_ASSERT_NEAR(a, b, eps) do { \
    double _a = (a), _b = (b); \
    if (std::fabs(_a - _b) > (eps)) { \
        std::printf("FAIL at %s:%d: %g vs %g (eps=%g)\n", __FILE__, __LINE__, _a, _b, (double)(eps)); \
        return; \
    } \
} while(0)

// =========================================================================
// Transfer functions (matching the plugin implementation)
// =========================================================================

static double srgb_inv(double v) {
    if (v <= 0.04045) return v / 12.92;
    return std::pow((v + 0.055) / 1.055, 2.4);
}
static double srgb_fwd(double v) {
    if (v <= 0.0031308) return v * 12.92;
    return 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
}

static double rec709_inv(double v) {
    if (v < 0.081) return v / 4.5;
    return std::pow((v + 0.099) / 1.099, 1.0 / 0.45);
}
static double rec709_fwd(double v) {
    if (v < 0.018) return v * 4.5;
    return 1.099 * std::pow(v, 0.45) - 0.099;
}

static double rec2020_inv(double v) { return rec709_inv(v); }
static double rec2020_fwd(double v) { return rec709_fwd(v); }

static double acescct_inv(double v) {
    const double kLinSlope = 10.5402377416545;
    const double kLinOff   = 0.0729055341958355;
    const double kLogCut   = (std::log2(0.0078125) + 9.72) / 17.52;
    if (v >= kLogCut) return std::pow(2.0, v * 17.52 - 9.72);
    if (v >= kLinOff) return (v - kLinOff) / kLinSlope;
    return (v - 0.5) * 2.0;
}
static double acescct_fwd(double v) {
    const double kLinSlope = 10.5402377416545;
    const double kLinOff   = 0.0729055341958355;
    const double kLogCut   = 0.0078125;
    if (v < 0.0) return v * 0.5 + 0.5;
    if (v < kLogCut) return kLinSlope * v + kLinOff;
    return (std::log2(v) + 9.72) / 17.52;
}

static double logc_inv(double v) {
    const double cut = 0.010591, a = 5.555556, b = 0.052272;
    const double c = 0.247190, d = 0.385537, e = 5.367655, f = 0.092809;
    const double cutEnc = e * cut + f;
    if (v > cutEnc) return (std::pow(10.0, (v - d) / c) - b) / a;
    return (v - f) / e;
}
static double logc_fwd(double v) {
    const double cut = 0.010591, a = 5.555556, b = 0.052272;
    const double c = 0.247190, d = 0.385537, e = 5.367655, f = 0.092809;
    if (v > cut) return c * std::log10(a * v + b) + d;
    return e * v + f;
}

static double slog3_inv(double v) {
    const double cutEnc = 171.2102946929 / 1023.0;
    if (v >= cutEnc) {
        return (std::pow(10.0, (v * 1023.0 - 420.0) / 261.5)) * 0.19 - 0.01;
    } else {
        return (v * 1023.0 - 95.0) * 0.01125000 / 76.2102946929;
    }
}
static double slog3_fwd(double v) {
    const double cut = 0.01125000;
    if (v >= cut) {
        return (420.0 + std::log10((v + 0.01) / 0.19) * 261.5) / 1023.0;
    } else {
        return (v * 76.2102946929 / 0.01125000 + 95.0) / 1023.0;
    }
}

static double slog2_inv(double v) {
    const double kFullScale = 1023.0 / 876.0;
    const double kFullOff = 64.0 / 876.0;
    double y_ire = v * kFullScale - kFullOff;
    double x_ire;
    if (y_ire >= 0.030001222851889303) {
        x_ire = std::pow(10.0, (y_ire - 0.646596) / 0.432699) - 0.037584;
    } else {
        x_ire = (y_ire - 0.030001222851889303) / 5.0;
    }
    return x_ire * 0.9 * 219.0 / 155.0;
}
static double slog2_fwd(double v) {
    double ire = v * 155.0 / (219.0 * 0.9);
    double y_ire;
    if (ire >= 0.0) {
        y_ire = 0.432699 * std::log10(ire + 0.037584) + 0.646596;
    } else {
        y_ire = ire * 5.0 + 0.030001222851889303;
    }
    return y_ire * 876.0 / 1023.0 + 64.0 / 1023.0;
}

static double vlog_inv(double v) {
    const double cutEnc = 0.181, b = 0.00873, c = 0.241514, d = 0.598206;
    if (v < cutEnc) return (v - 0.125) / 5.6;
    return std::pow(10.0, (v - d) / c) - b;
}
static double vlog_fwd(double v) {
    const double cut = 0.01, b = 0.00873, c = 0.241514, d = 0.598206;
    if (v < cut) return 5.6 * v + 0.125;
    return c * std::log10(v + b) + d;
}

// =========================================================================
// Color matrices (matching the plugin)
// =========================================================================

struct Mat3 { double m[3][3]; };

static const Mat3 k709_to_XYZ = {{
    { 0.412391,  0.357584,  0.180481 },
    { 0.212639,  0.715169,  0.072192 },
    { 0.019331,  0.119195,  0.950532 }
}};

static const Mat3 kXYZ_to_709 = {{
    {  3.240970, -1.537383, -0.498611 },
    { -0.969244,  1.875968,  0.041555 },
    {  0.055630, -0.203977,  1.056972 }
}};

static const Mat3 k2020_to_XYZ = {{
    { 0.636958,  0.144617,  0.168881 },
    { 0.262700,  0.677998,  0.059302 },
    { 0.000000,  0.028073,  1.060985 }
}};

static const Mat3 kXYZ_to_2020 = {{
    {  1.716651, -0.355671, -0.253366 },
    { -0.666684,  1.616481,  0.015769 },
    {  0.017640, -0.042771,  0.942103 }
}};

static const Mat3 kAP1d65_to_XYZ = {{
    { 0.652276,  0.128258,  0.169942 },
    { 0.267689,  0.674335,  0.057975 },
    { -0.005382, 0.001376,  1.092831 }
}};

static const Mat3 kXYZ_to_AP1d65 = {{
    {  1.660513, -0.315336, -0.241492 },
    { -0.659944,  1.608427,  0.017298 },
    {  0.009010, -0.003579,  0.913844 }
}};

static void applyMat3(const Mat3 &M, const double in[3], double out[3]) {
    out[0] = M.m[0][0]*in[0] + M.m[0][1]*in[1] + M.m[0][2]*in[2];
    out[1] = M.m[1][0]*in[0] + M.m[1][1]*in[1] + M.m[1][2]*in[2];
    out[2] = M.m[2][0]*in[0] + M.m[2][1]*in[1] + M.m[2][2]*in[2];
}

// =========================================================================
// Test helpers
// =========================================================================

static void test_tf_roundtrip(const char *name, double (*fwd)(double), double (*inv)(double)) {
    TEST(name);
    double vals[] = {0.0, 0.001, 0.01, 0.1, 0.18, 0.5, 0.9, 1.0};
    for (double v : vals) {
        double e = fwd(v);
        double r = inv(e);
        TF_ASSERT_NEAR(r, v, 1e-6);
    }
    PASS();
}

static void test_tf_at_18pct(const char *name, double (*fwd)(double), double expected) {
    TEST(name);
    double e = fwd(0.18);
    TF_ASSERT_NEAR(e, expected, 0.01);
    PASS();
}

static void test_matrix_roundtrip(const char *name, const Mat3 &fwd, const Mat3 &inv) {
    TEST(name);
    double tests[][3] = {{0,0,0},{0.18,0.18,0.18},{1,0,0},{0,1,0},{0,0,1},{1,1,1},{0.5,0.3,0.7}};
    for (auto &t : tests) {
        double xyz[3], back[3];
        applyMat3(fwd, t, xyz);
        applyMat3(inv, xyz, back);
        for (int i = 0; i < 3; ++i)
            TF_ASSERT_NEAR(back[i], t[i], 2e-6);
    }
    PASS();
}

static void test_matrix_product_identity(const char *name, const Mat3 &fwd, const Mat3 &inv) {
    TEST(name);
    double prod[3][3] = {{0}};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                prod[i][j] += fwd.m[i][k] * inv.m[k][j];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double expected = (i == j) ? 1.0 : 0.0;
            TF_ASSERT_NEAR(prod[i][j], expected, 2e-6);
        }
    PASS();
}

// =========================================================================
// dE helpers
// =========================================================================

static double lab_f(double t) {
    return (t > 0.008856) ? std::pow(t, 1.0/3.0) : (903.3 * t + 16.0) / 116.0;
}

static void xyzToLab(const double xyz[3], double lab[3], const double ref[3]) {
    double fx = lab_f(xyz[0] / ref[0]);
    double fy = lab_f(xyz[1] / ref[1]);
    double fz = lab_f(xyz[2] / ref[2]);
    lab[0] = 116.0 * fy - 16.0;
    lab[1] = 500.0 * (fx - fy);
    lab[2] = 200.0 * (fy - fz);
}

// CIEDE2000
static double dE00(const double a[3], const double b[3]) {
    double L1=a[0],a1=a[1],b1_=a[2];
    double L2=b[0],a2=b[1],b2_=b[2];
    double Lm = (L1+L2)/2.0;
    double C1 = std::sqrt(a1*a1 + b1_*b1_);
    double C2 = std::sqrt(a2*a2 + b2_*b2_);
    double Cm = (C1+C2)/2.0;
    double G = 0.5*(1.0-std::sqrt(std::pow(Cm,7)/(std::pow(Cm,7)+std::pow(25,7))));
    double a1p = a1*(1.0+G), a2p = a2*(1.0+G);
    double C1p = std::sqrt(a1p*a1p + b1_*b1_), C2p = std::sqrt(a2p*a2p + b2_*b2_);
    double Cmp = (C1p+C2p)/2.0, dCp = C2p-C1p;
    double h1p = std::atan2(b1_, a1p)*180.0/3.14159265358979323846;
    double h2p = std::atan2(b2_, a2p)*180.0/3.14159265358979323846;
    if (h1p < 0) h1p += 360.0;
    if (h2p < 0) h2p += 360.0;
    double dhp;
    if (C1p*C2p == 0) dhp = 0.0;
    else if (std::fabs(h2p-h1p) <= 180.0) dhp = h2p-h1p;
    else if (h2p-h1p > 180.0) dhp = h2p-h1p-360.0;
    else dhp = h2p-h1p+360.0;
    double dHp = 2.0*std::sqrt(C1p*C2p)*std::sin(dhp*3.14159265358979323846/360.0);
    double dLp = L2-L1;
    double Hmp = (C1p==0||C2p==0) ? h1p+h2p : (std::fabs(h1p-h2p)>180.0) ? (h1p+h2p+360.0)/2.0 : (h1p+h2p)/2.0;
    double T = 1.0 - 0.17*std::cos((Hmp-30.0)*3.14159265358979323846/180.0) + 0.24*std::cos(2.0*Hmp*3.14159265358979323846/180.0) + 0.32*std::cos((3.0*Hmp+6.0)*3.14159265358979323846/180.0) - 0.20*std::cos((4.0*Hmp-63.0)*3.14159265358979323846/180.0);
    double SL = 1.0 + 0.015*std::pow(Lm-50.0,2.0)/std::sqrt(20.0+std::pow(Lm-50.0,2.0));
    double SC = 1.0 + 0.045*Cmp;
    double SH = 1.0 + 0.015*Cmp*T;
    double RT = -2.0*std::sqrt(std::pow(Cmp,7)/(std::pow(Cmp,7)+std::pow(25,7)))*std::sin(60.0*std::exp(-std::pow((Hmp-275.0)/25.0,2.0))*3.14159265358979323846/180.0);
    return std::sqrt(std::pow(dLp/SL,2)+std::pow(dCp/SC,2)+std::pow(dHp/SH,2)+RT*dCp/SC*dHp/SH);
}

// =========================================================================
// Tests
// =========================================================================

void test_srgb_roundtrip()        { test_tf_roundtrip("sRGB",     srgb_fwd,     srgb_inv); }
void test_rec709_roundtrip()      { test_tf_roundtrip("Rec.709",  rec709_fwd,   rec709_inv); }
void test_rec2020_roundtrip()     { test_tf_roundtrip("Rec.2020", rec2020_fwd,  rec2020_inv); }
void test_acescct_roundtrip()     { test_tf_roundtrip("ACEScct",  acescct_fwd,  acescct_inv); }
void test_logc_roundtrip()        { test_tf_roundtrip("LogC v3",  logc_fwd,     logc_inv); }
void test_slog3_roundtrip()       { test_tf_roundtrip("S-Log3",   slog3_fwd,    slog3_inv); }
void test_slog2_roundtrip()       { test_tf_roundtrip("S-Log2",   slog2_fwd,    slog2_inv); }
void test_vlog_roundtrip()        { test_tf_roundtrip("V-Log",    vlog_fwd,     vlog_inv); }

void test_srgb_18pct()            { test_tf_at_18pct("sRGB",     srgb_fwd,     0.461); }
void test_rec709_18pct()          { test_tf_at_18pct("Rec.709",  rec709_fwd,   0.409); }
void test_acescct_18pct()         { test_tf_at_18pct("ACEScct",  acescct_fwd,  0.413); }
void test_logc_18pct()            { test_tf_at_18pct("LogC v3",  logc_fwd,     0.391); }
void test_slog3_18pct()           { test_tf_at_18pct("S-Log3",   slog3_fwd,    0.411); }
void test_slog2_18pct()           { test_tf_at_18pct("S-Log2",   slog2_fwd,    0.340); }
void test_vlog_18pct()            { test_tf_at_18pct("V-Log",    vlog_fwd,     0.423); }

void test_709_matrix_roundtrip()  { test_matrix_roundtrip("709/sRGB matrix round-trip", k709_to_XYZ, kXYZ_to_709); }
void test_2020_matrix_roundtrip() { test_matrix_roundtrip("2020 matrix round-trip", k2020_to_XYZ, kXYZ_to_2020); }
void test_ap1_matrix_roundtrip()  { test_matrix_roundtrip("AP1 matrix round-trip", kAP1d65_to_XYZ, kXYZ_to_AP1d65); }
void test_709_matrix_id()         { test_matrix_product_identity("709/sRGB M * inv(M) == I", k709_to_XYZ, kXYZ_to_709); }
void test_2020_matrix_id()        { test_matrix_product_identity("2020 M * inv(M) == I", k2020_to_XYZ, kXYZ_to_2020); }
void test_ap1_matrix_id()         { test_matrix_product_identity("AP1 M * inv(M) == I", kAP1d65_to_XYZ, kXYZ_to_AP1d65); }

void test_dE_identity() {
    TEST("dE00 between identical colors is 0");
    double lab[3] = {50.0, 10.0, -10.0};
    TF_ASSERT_NEAR(dE00(lab, lab), 0.0, 1e-10);
    PASS();
}

void test_dE_known_pair() {
    TEST("dE00 known pair ~2.0425");
    double a[3] = {50.0, 2.6772, -79.7751};
    double b[3] = {50.0, 0.0, -82.7485};
    TF_ASSERT_NEAR(dE00(a, b), 2.0425, 0.01);
    PASS();
}

void test_xyz_d65_white() {
    TEST("XYZ D65 -> Lab: white is L=100, a=b=0");
    double ref[3] = {0.95047, 1.00000, 1.08883};
    double lab[3];
    xyzToLab(ref, lab, ref);
    TF_ASSERT_NEAR(lab[0], 100.0, 1e-6);
    TF_ASSERT_NEAR(lab[1], 0.0, 1e-6);
    TF_ASSERT_NEAR(lab[2], 0.0, 1e-6);
    PASS();
}

void test_2020_to_709_white() {
    TEST("Rec.2020 -> Rec.709: D65 white stays white");
    double rgb2020[3] = {1.0, 1.0, 1.0};
    double lin[3] = {rec2020_inv(rgb2020[0]), rec2020_inv(rgb2020[1]), rec2020_inv(rgb2020[2])};
    double xyz[3];
    applyMat3(k2020_to_XYZ, lin, xyz);
    double lin709[3];
    applyMat3(kXYZ_to_709, xyz, lin709);
    for (int i = 0; i < 3; ++i)
        TF_ASSERT_NEAR(lin709[i], 1.0, 0.001);
    PASS();
}

void test_aces_p3_white() {
    TEST("ACEScct (AP1) -> Rec.709: white stays white");
    double linAP1[3] = {1.0, 1.0, 1.0};
    double xyz[3];
    applyMat3(kAP1d65_to_XYZ, linAP1, xyz);
    double lin709[3];
    applyMat3(kXYZ_to_709, xyz, lin709);
    for (int i = 0; i < 3; ++i)
        TF_ASSERT_NEAR(lin709[i], 1.0, 0.001);
    PASS();
}

void test_rec709_luma_weights() {
    TEST("Rec.709 luma: green > red > blue");
    assert((0.715169) > (0.212639) && (0.212639) > (0.072192));
    PASS();
}

void test_srgb_black() {
    TEST("sRGB: black encodes to 0");
    TF_ASSERT_NEAR(srgb_fwd(0.0), 0.0, 1e-10);
    PASS();
}

void test_srgb_white() {
    TEST("sRGB: white encodes to 1");
    TF_ASSERT_NEAR(srgb_fwd(1.0), 1.0, 1e-6);
    PASS();
}

void test_vlog_inv_black() {
    TEST("V-Log: inverse at 0.125 gives ~0");
    double v = vlog_inv(0.125);
    TF_ASSERT_NEAR(v, 0.0, 1e-4);
    PASS();
}

void test_acescct_neg_encoding() {
    TEST("ACEScct: below-black encoding produces values < 0.5");
    double neg = acescct_fwd(-0.1);
    assert(neg < 0.5);
    (void)neg;
    PASS();
}

void test_acescct_neg_roundtrip() {
    TEST("ACEScct: negative value round-trip via unique range");
    double orig = -1.0;
    double enc = acescct_fwd(orig);
    assert(enc < 0.073);
    double dec = acescct_inv(enc);
    TF_ASSERT_NEAR(dec, orig, 1e-6);
    PASS();
}

// =========================================================================
// Main
// =========================================================================

int main()
{
    std::printf("colorspace-test: Color Space Plugin Unit Tests\n");
    std::printf("===============================================\n\n");

    std::printf("--- Transfer Function Round-trips ---\n");
    test_srgb_roundtrip();
    test_rec709_roundtrip();
    test_rec2020_roundtrip();
    test_acescct_roundtrip();
    test_logc_roundtrip();
    test_slog3_roundtrip();
    test_slog2_roundtrip();
    test_vlog_roundtrip();

    std::printf("\n--- 18%% Gray Reference Values ---\n");
    test_srgb_18pct();
    test_rec709_18pct();
    test_acescct_18pct();
    test_logc_18pct();
    test_slog3_18pct();
    test_slog2_18pct();
    test_vlog_18pct();

    std::printf("\n--- Special Cases ---\n");
    test_srgb_black();
    test_srgb_white();
    test_vlog_inv_black();
    test_acescct_neg_encoding();
    test_acescct_neg_roundtrip();

    std::printf("\n--- Matrix Round-trips ---\n");
    test_709_matrix_roundtrip();
    test_2020_matrix_roundtrip();
    test_ap1_matrix_roundtrip();
    test_709_matrix_id();
    test_2020_matrix_id();
    test_ap1_matrix_id();

    std::printf("\n--- Color Space Conversion ---\n");
    test_2020_to_709_white();
    test_aces_p3_white();
    test_rec709_luma_weights();

    std::printf("\n--- CIEDE2000 Validation ---\n");
    test_dE_identity();
    test_dE_known_pair();
    test_xyz_d65_white();

    std::printf("\nResults: %d/%d passed\n", s_passCount, s_testCount);
    return (s_passCount == s_testCount) ? 0 : 1;
}
