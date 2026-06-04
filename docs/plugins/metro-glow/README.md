# metro-glow

Bloom/glow effect with threshold control, configurable radius, and tint color. GPU-accelerated with CUDA box blur.

## Parameters

| Parameter | Type   | Default | Description                     |
|-----------|--------|---------|---------------------------------|
| Intensity | float  | 0.5     | Glow brightness                 |
| Threshold | float  | 0.8     | Luminance threshold for glow    |
| Radius    | float  | 10.0    | Blur radius (pixels)            |
| GlowColor | color  | 1,1,1   | Glow tint color                 |
| Mix       | float  | 1.0     | Effect blend amount             |

## GPU acceleration

Uses CUDA kernels for fast box blur and compositing (`GlowKernels.cu`).
