#include "FilmGrainKernels.cuh"
#include <cstdio>

extern "C" {

// --- Device helpers ---

__device__ static inline unsigned int hashUInt(unsigned int x, unsigned int y, unsigned int seed)
{
    unsigned int h = x * 374761393u + y * 668265263u + seed * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return h;
}

__device__ static inline float hashFloat(unsigned int x, unsigned int y, unsigned int seed)
{
    return float(hashUInt(x, y, seed)) / 4294967295.0f;
}

__device__ static inline float smoothstep5(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// --- Film grain noise generation kernel ---
// Generates a single-channel grain texture using value noise with
// grain size scaling and sharpness contrast shaping.

__global__ void generateGrainKernel(
    float* grain,
    int w,
    int h,
    float grainSize,
    float sharpness,
    unsigned int seed)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) return;

    float invSize = 1.0f / fmaxf(grainSize, 0.001f);
    float fx = float(x) * invSize;
    float fy = float(y) * invSize;

    int ix = int(floorf(fx));
    int iy = int(floorf(fy));
    float fracX = fx - float(ix);
    float fracY = fy - float(iy);

    float sx = smoothstep5(fracX);
    float sy = smoothstep5(fracY);

    float v00 = hashFloat(ix, iy, seed);
    float v10 = hashFloat(ix + 1, iy, seed);
    float v01 = hashFloat(ix, iy + 1, seed);
    float v11 = hashFloat(ix + 1, iy + 1, seed);

    float v0 = v00 + sx * (v10 - v00);
    float v1 = v01 + sx * (v11 - v01);
    float noise = v0 + sy * (v1 - v0);

    float centered = noise * 2.0f - 1.0f;
    float p = 1.0f / fmaxf(0.1f, 1.0f + sharpness * 3.0f);
    float shaped = powf(fabsf(centered), p);
    if (centered < 0.0f) shaped = -shaped;
    float result = shaped * 0.5f + 0.5f;

    grain[y * w + x] = fminf(1.0f, fmaxf(0.0f, result));
}

// --- Grain application kernel ---
// Blends the grain texture onto the source image.
// colorMode=0: luma-dependent grain (same value per channel)
// colorMode=1: independent per-channel grain

__global__ void applyGrainKernel(
    const float* src,
    float* dst,
    const float* grain,
    int w,
    int h,
    int srcStride,
    int dstStride,
    float intensity,
    float mix,
    int colorMode,
    unsigned int seed)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) return;

    int si = y * srcStride + x * 4;
    int di = y * dstStride + x * 4;

    float r = src[si + 0];
    float g = src[si + 1];
    float b = src[si + 2];
    float a = src[si + 3];

    if (colorMode) {
        float gr = (hashFloat(x, y, seed + 0) * 0.3f + grain[y * w + x] * 0.7f) * 2.0f - 1.0f;
        float gg = (hashFloat(x, y, seed + 1) * 0.3f + grain[y * w + x] * 0.7f) * 2.0f - 1.0f;
        float gb = (hashFloat(x, y, seed + 2) * 0.3f + grain[y * w + x] * 0.7f) * 2.0f - 1.0f;

        float nr = fminf(1.0f, fmaxf(0.0f, r * (1.0f + gr * intensity)));
        float ng = fminf(1.0f, fmaxf(0.0f, g * (1.0f + gg * intensity)));
        float nb = fminf(1.0f, fmaxf(0.0f, b * (1.0f + gb * intensity)));

        dst[di + 0] = r + mix * (nr - r);
        dst[di + 1] = g + mix * (ng - g);
        dst[di + 2] = b + mix * (nb - b);
    } else {
        float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        float gv = (grain[y * w + x] * 2.0f - 1.0f) * intensity * (1.0f - luma * 0.5f);
        float nr = fminf(1.0f, fmaxf(0.0f, r * (1.0f + gv)));
        float ng = fminf(1.0f, fmaxf(0.0f, g * (1.0f + gv)));
        float nb = fminf(1.0f, fmaxf(0.0f, b * (1.0f + gv)));

        dst[di + 0] = r + mix * (nr - r);
        dst[di + 1] = g + mix * (ng - g);
        dst[di + 2] = b + mix * (nb - b);
    }
    dst[di + 3] = a;
}

// --- Wrapper functions ---

bool launchGenerateGrain(float* grain, int w, int h, float grainSize, float sharpness, unsigned int seed)
{
    if (!grain || w <= 0 || h <= 0) return false;

    dim3 block(16, 16);
    dim3 grid((w + block.x - 1) / block.x, (h + block.y - 1) / block.y);

    generateGrainKernel<<<grid, block>>>(grain, w, h, grainSize, sharpness, seed);

    cudaError_t err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        fprintf(stderr, "FilmGrain: generateGrainKernel failed: %s\n", cudaGetErrorString(err));
        return false;
    }
    return true;
}

bool launchApplyGrain(
    const float* src, float* dst, const float* grain,
    int w, int h, int srcStride, int dstStride,
    float intensity, float mix, int colorMode, unsigned int seed)
{
    if (!src || !dst || !grain || w <= 0 || h <= 0) return false;

    dim3 block(16, 16);
    dim3 grid((w + block.x - 1) / block.x, (h + block.y - 1) / block.y);

    applyGrainKernel<<<grid, block>>>(src, dst, grain, w, h, srcStride, dstStride, intensity, mix, colorMode, seed);

    cudaError_t err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        fprintf(stderr, "FilmGrain: applyGrainKernel failed: %s\n", cudaGetErrorString(err));
        return false;
    }
    return true;
}

} // extern "C"
