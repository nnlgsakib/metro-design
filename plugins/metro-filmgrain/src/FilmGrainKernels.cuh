#pragma once

bool launchGenerateGrain(float* grain, int w, int h, float grainSize, float sharpness, unsigned int seed);
bool launchApplyGrain(const float* src, float* dst, const float* grain, int w, int h, int srcStride, int dstStride, float intensity, float mix, int colorMode, unsigned int seed);
