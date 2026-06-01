#pragma once

struct FlareParams {
    float brightness;
    float flareSize;
    int ghostCount;
    float anamorphicStretch;
    float chromaShift;
    float hueTint;
    float mix;
    float centerX;
    float centerY;
    float imgCx;
    float imgCy;
    float axisDx;
    float axisDy;
    float axisLen;
};

bool launchLensFlareGPU(const float *src, float *dst,
                        int srcStride, int dstStride,
                        int rx1, int ry1, int rx2, int ry2,
                        int srcW, int srcH,
                        const FlareParams &params);
