# metro-lensflare

Cinematic lens flare with configurable ghost count, anamorphic stretch, chromatic shift, and hue tinting. GPU-accelerated with CUDA.

## Parameters

| Parameter         | Type   | Default | Description                   |
|-------------------|--------|---------|-------------------------------|
| Brightness        | float  | 1.0     | Overall flare brightness      |
| FlareSize         | float  | 1.0     | Flare element scale           |
| GhostCount        | int    | 5       | Number of ghost elements      |
| AnamorphicStretch | float  | 0.0     | Anamorphic stretch factor     |
| ChromaShift       | float  | 0.0     | Chromatic aberration on flare |
| HueTint           | float  | 0.0     | Flare hue shift               |
| Mix               | float  | 1.0     | Effect blend amount           |

## GPU acceleration

Uses CUDA kernels for procedural flare rendering (`LensFlareKernels.cu`).
