#include "LensFlareKernels.hpp"
#include <cstdio>
#include <cuda_runtime.h>

__device__ float flare_gaussian(float x, float sigma)
{
    return expf(-(x * x) / (2.0f * sigma * sigma));
}

__device__ void hsv_to_rgb(float h, float s, float v, float &r, float &g, float &b)
{
    float c = v * s;
    float hp = h / 60.0f;
    float x = c * (1.0f - fabsf(__fmodf(hp, 2.0f) - 1.0f));
    float m = v - c;
    int hi = ((int)hp) % 6;
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

__global__ void findMaxLum_kernel(const float *src, int srcStride, int w, int h, int nc,
                                  float *outLum, float *outX, float *outY)
{
    extern __shared__ float shared[];
    float *sLum = shared;
    float *sX = &shared[blockDim.x * blockDim.y];
    float *sY = &shared[blockDim.x * blockDim.y * 2];

    int tid = threadIdx.y * blockDim.x + threadIdx.x;
    int gx = blockIdx.x * blockDim.x + threadIdx.x;
    int gy = blockIdx.y * blockDim.y + threadIdx.y;

    float myLum = -1.0f;
    float myX = 0.0f, myY = 0.0f;
    if (gx < w && gy < h) {
        int si = gy * srcStride + gx * nc;
        myLum = 0.2126f * src[si + 0] + 0.7152f * src[si + 1] + 0.0722f * src[si + 2];
        myX = static_cast<float>(gx);
        myY = static_cast<float>(gy);
    }

    sLum[tid] = myLum;
    sX[tid] = myX;
    sY[tid] = myY;
    __syncthreads();

    for (int s = (blockDim.x * blockDim.y) / 2; s > 0; s >>= 1) {
        if (tid < s) {
            if (sLum[tid + s] > sLum[tid]) {
                sLum[tid] = sLum[tid + s];
                sX[tid] = sX[tid + s];
                sY[tid] = sY[tid + s];
            }
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicMax((int *)outLum, __float_as_int(sLum[0]));
        if (sLum[0] >= __int_as_float(atomicAdd((int *)outLum, 0))) {
            *outX = sX[0];
            *outY = sY[0];
        }
    }
}

__global__ void lensFlare_kernel(const float *src, float *dst,
                                 int srcStride, int dstStride,
                                 int w, int h, int nc,
                                 float brightness, float flareSize,
                                 int ghostCount, float anamorphicStretch,
                                 float chromaShift,
                                 float tintR, float tintG, float tintB,
                                 float mix,
                                 float cfx, float cfy,
                                 float imgCx, float imgCy)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) return;

    int si = y * srcStride + x * nc;
    int di = y * dstStride + x * nc;

    float origR = src[si + 0];
    float origG = src[si + 1];
    float origB = src[si + 2];
    float origA = src[si + 3];

    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);

    float axisDx = imgCx - cfx;
    float axisDy = imgCy - cfy;
    float axisLen = sqrtf(axisDx * axisDx + axisDy * axisDy);
    if (axisLen < 0.001f) {
        axisDx = 1.0f; axisDy = 0.0f;
    } else {
        axisDx /= axisLen; axisDy /= axisLen;
    }

    float dx = fx - cfx;
    float dy = fy - cfy;
    float dist = sqrtf(dx * dx + dy * dy);

    float flareR = 0.0f, flareG = 0.0f, flareB = 0.0f;

    if (brightness > 0.0f) {
        float centralSigma = fmaxf(8.0f * flareSize, 1.0f);
        float centralIntensity = brightness * flare_gaussian(dist, centralSigma);
        flareR += centralIntensity;
        flareG += centralIntensity;
        flareB += centralIntensity;

        if (anamorphicStretch > 0.0f) {
            float perpDist = fabsf(dx * (-axisDy) + dy * axisDx);
            float streakSigma = fmaxf(4.0f * flareSize, 1.0f);
            float streakIntensity = brightness * anamorphicStretch * flare_gaussian(perpDist, streakSigma);
            flareR += streakIntensity;
            flareG += streakIntensity;
            flareB += streakIntensity;
        }

        float ghostSpacing = fmaxf(30.0f * flareSize, 5.0f);
        for (int gi = 0; gi < ghostCount; ++gi) {
            float t = static_cast<float>(gi + 1) * 0.18f;
            float gx = cfx + axisDx * ghostSpacing * t;
            float gy = cfy + axisDy * ghostSpacing * t;
            float gdx = fx - gx;
            float gdy = fy - gy;
            float gdist = sqrtf(gdx * gdx + gdy * gdy);

            float ghostSigma = fmaxf(10.0f * flareSize * (1.0f + t * 0.5f), 1.0f);
            float ghostIntensity = brightness * flare_gaussian(gdist, ghostSigma) * (0.7f - static_cast<float>(gi) * 0.06f);

            if (ghostIntensity > 0.0f) {
                float gr = ghostIntensity;
                float gg = ghostIntensity;
                float gb = ghostIntensity;

                if (chromaShift > 0.0f) {
                    float chromaPx = chromaShift * 6.0f * flareSize;
                    float clum = 0.2126f * gr + 0.7152f * gg + 0.0722f * gb;
                    gr = ghostIntensity * (clum + (gr - clum) * chromaShift);
                    gg = ghostIntensity * (clum + (gg - clum) * chromaShift);
                    gb = ghostIntensity * (clum + (gb - clum) * chromaShift);
                }

                float tintBlend = 0.3f;
                flareR += gr * (1.0f - tintBlend) + gr * tintBlend * tintR;
                flareG += gg * (1.0f - tintBlend) + gg * tintBlend * tintG;
                flareB += gb * (1.0f - tintBlend) + gb * tintBlend * tintB;
            }
        }
    }

    float outR = origR + mix * flareR;
    float outG = origG + mix * flareG;
    float outB = origB + mix * flareB;

    dst[di + 0] = outR;
    dst[di + 1] = outG;
    dst[di + 2] = outB;
    dst[di + 3] = origA;
}

bool launchLensFlareGPU(const float *src, float *dst,
                        int srcStride, int dstStride,
                        int rx1, int ry1, int rx2, int ry2,
                        int srcW, int srcH,
                        const FlareParams &params)
{
    int w = rx2 - rx1;
    int h = ry2 - ry1;
    if (w <= 0 || h <= 0) return false;

    const int nc = 4;
    size_t bufSize = static_cast<size_t>(w) * h * nc * sizeof(float);

    float *d_dst = nullptr;
    cudaError_t err = cudaMalloc(&d_dst, bufSize);
    if (err != cudaSuccess) return false;

    float *d_src = nullptr;
    err = cudaMalloc(&d_src, bufSize);
    if (err != cudaSuccess) {
        cudaFree(d_dst);
        return false;
    }

    err = cudaMemcpy(d_src, src + ry1 * srcStride + rx1 * nc, bufSize, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        cudaFree(d_src);
        cudaFree(d_dst);
        return false;
    }

    float *d_maxLum = nullptr, *d_maxX = nullptr, *d_maxY = nullptr;
    cudaMalloc(&d_maxLum, sizeof(float));
    cudaMalloc(&d_maxX, sizeof(float));
    cudaMalloc(&d_maxY, sizeof(float));
    float negOne = -1.0f;
    cudaMemcpy(d_maxLum, &negOne, sizeof(float), cudaMemcpyHostToDevice);

    dim3 block(16, 16);
    dim3 grid((w + 15) / 16, (h + 15) / 16);

    size_t sharedBytes = 3 * block.x * block.y * sizeof(float);
    findMaxLum_kernel<<<grid, block, sharedBytes>>>(d_src, srcStride, w, h, nc, d_maxLum, d_maxX, d_maxY);
    cudaDeviceSynchronize();

    float maxLum = 0.0f, cfx = 0.0f, cfy = 0.0f;
    cudaMemcpy(&maxLum, d_maxLum, sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(&cfx, d_maxX, sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(&cfy, d_maxY, sizeof(float), cudaMemcpyDeviceToHost);

    if (maxLum < 0.0f) {
        cfx = static_cast<float>(w) * 0.5f;
        cfy = static_cast<float>(h) * 0.5f;
    }

    float imgCx = static_cast<float>(w) * 0.5f;
    float imgCy = static_cast<float>(h) * 0.5f;

    float tintR, tintG, tintB;
    hsv_to_rgb(static_cast<float>(params.hueTint), 1.0f, 1.0f, tintR, tintG, tintB);

    lensFlare_kernel<<<grid, block>>>(
        d_src, d_dst, srcStride, w * nc, w, h, nc,
        params.brightness, params.flareSize, params.ghostCount,
        params.anamorphicStretch, params.chromaShift,
        tintR, tintG, tintB, params.mix,
        cfx, cfy, imgCx, imgCy
    );
    cudaDeviceSynchronize();

    err = cudaMemcpy(dst + ry1 * dstStride + rx1 * nc, d_dst, bufSize, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        cudaFree(d_src); cudaFree(d_dst);
        cudaFree(d_maxLum); cudaFree(d_maxX); cudaFree(d_maxY);
        return false;
    }

    cudaFree(d_src); cudaFree(d_dst);
    cudaFree(d_maxLum); cudaFree(d_maxX); cudaFree(d_maxY);
    return true;
}
