#include "GlowKernels.hpp"
#include <cstdio>
#include <cuda_runtime.h>

__global__ void glowThreshold_kernel(const float *src, float *dst,
                                     int srcRowStride, int dstRowStride,
                                     int w, int h, int nc,
                                     float threshold)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) return;

    int si = y * srcRowStride + x * nc;
    int di = y * dstRowStride + x * nc;
    float luma = 0.2126f * src[si + 0] + 0.7152f * src[si + 1] + 0.0722f * src[si + 2];
    float bright = fmaxf(0.0f, luma - threshold) / (1.0f - threshold + 1e-6f);
    dst[di + 0] = src[si + 0] * bright;
    dst[di + 1] = src[si + 1] * bright;
    dst[di + 2] = src[si + 2] * bright;
    dst[di + 3] = src[si + 3];
}

__global__ void blurH_kernel(const float *src, float *dst,
                             int rowStride, int w, int h, int nc, int radius)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) return;

    float inv = 1.0f / (2.0f * radius + 1.0f);
    for (int c = 0; c < nc; ++c) {
        float sum = 0.0f;
        for (int dx = -radius; dx <= radius; ++dx) {
            int ix = min(max(0, x + dx), w - 1);
            sum += src[y * rowStride + ix * nc + c];
        }
        dst[y * rowStride + x * nc + c] = sum * inv;
    }
}

__global__ void blurV_kernel(const float *src, float *dst,
                             int rowStride, int w, int h, int nc, int radius)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) return;

    float inv = 1.0f / (2.0f * radius + 1.0f);
    for (int c = 0; c < nc; ++c) {
        float sum = 0.0f;
        for (int dy = -radius; dy <= radius; ++dy) {
            int iy = min(max(0, y + dy), h - 1);
            sum += src[iy * rowStride + x * nc + c];
        }
        dst[y * rowStride + x * nc + c] = sum * inv;
    }
}

__global__ void composite_kernel(const float *orig, const float *glow,
                                 float *dst,
                                 int origStride, int glowStride, int dstStride,
                                 int w, int h, int nc,
                                 float intensity, float mix,
                                 float glowR, float glowG, float glowB)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) return;

    int oi = y * origStride + x * nc;
    int gi = y * glowStride + x * nc;
    int di = y * dstStride + x * nc;

    dst[di + 0] = orig[oi + 0] + mix * glow[gi + 0] * glowR * intensity;
    dst[di + 1] = orig[oi + 1] + mix * glow[gi + 1] * glowG * intensity;
    dst[di + 2] = orig[oi + 2] + mix * glow[gi + 2] * glowB * intensity;
    dst[di + 3] = orig[oi + 3];
}

bool launchGlowGPU(const float *src, float *dst,
                   int srcStride, int dstStride,
                   int rx1, int ry1, int rx2, int ry2,
                   float intensity, float threshold, float radius,
                   float glowR, float glowG, float glowB, float mix)
{
    int w = rx2 - rx1;
    int h = ry2 - ry1;
    if (w <= 0 || h <= 0) return false;

    const int nc = 4;
    size_t bufSize = static_cast<size_t>(w) * h * nc * sizeof(float);

    float *d_src = nullptr;
    float *d_bright = nullptr;
    float *d_tmp = nullptr;
    float *d_blur = nullptr;
    float *d_dst = nullptr;

    cudaError_t err;
    auto safeFree = [&]() {
        cudaFree(d_src); cudaFree(d_bright); cudaFree(d_tmp);
        cudaFree(d_blur); cudaFree(d_dst);
    };

    auto alloc = [&](float **p) -> bool {
        err = cudaMalloc(p, bufSize);
        return err == cudaSuccess;
    };

    if (!alloc(&d_src)) { safeFree(); return false; }
    if (!alloc(&d_bright)) { safeFree(); return false; }
    if (!alloc(&d_tmp)) { safeFree(); return false; }
    if (!alloc(&d_blur)) { safeFree(); return false; }
    if (!alloc(&d_dst)) { safeFree(); return false; }

    err = cudaMemcpy(d_src, src + ry1 * srcStride + rx1 * nc,
                     bufSize, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) { safeFree(); return false; }

    dim3 block(16, 16);
    dim3 grid((w + 15) / 16, (h + 15) / 16);

    glowThreshold_kernel<<<grid, block>>>(d_src, d_bright, srcStride, w * nc,
                                          w, h, nc, threshold);

    int r = static_cast<int>(radius + 0.5f);
    if (r < 1) r = 1;
    blurH_kernel<<<grid, block>>>(d_bright, d_tmp, w * nc, w, h, nc, r);
    blurV_kernel<<<grid, block>>>(d_tmp, d_blur, w * nc, w, h, nc, r);

    composite_kernel<<<grid, block>>>(d_src, d_blur, d_dst,
                                       srcStride, w * nc, dstStride,
                                       w, h, nc, intensity, mix,
                                       glowR, glowG, glowB);

    cudaDeviceSynchronize();

    err = cudaMemcpy(dst + ry1 * dstStride + rx1 * nc,
                     d_dst, bufSize, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) { safeFree(); return false; }

    safeFree();
    return true;
}
