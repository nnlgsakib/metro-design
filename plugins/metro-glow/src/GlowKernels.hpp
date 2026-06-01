#pragma once

bool launchGlowGPU(const float *src, float *dst,
                   int srcStride, int dstStride,
                   int rx1, int ry1, int rx2, int ry2,
                   float intensity, float threshold, float radius,
                   float glowR, float glowG, float glowB, float mix);
