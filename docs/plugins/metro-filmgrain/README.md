# metro-filmgrain

Analog film grain simulation with GPU acceleration. Supports configurable grain amount, size, sharpness, and multiple grain profiles.

## Parameters

| Parameter | Type   | Default | Description                     |
|-----------|--------|---------|---------------------------------|
| Amount    | float  | 0.5     | Grain intensity                 |
| Size      | float  | 1.0     | Grain particle size             |
| Sharpness | float  | 0.5     | Grain edge sharpness             |
| GrainType | enum   | 0       | Grain profile type              |
| Seed      | int    | 42      | Random seed for reproducibility |
| Mix       | float  | 1.0     | Effect blend amount             |

## GPU acceleration

Uses CUDA kernels for real-time grain generation and compositing (`FilmGrainKernels.cu`).
