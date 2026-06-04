# metro-chromab

Per-channel chromatic aberration effect with independent RGB shifting and radial falloff.

## Parameters

| Parameter     | Type  | Default | Description                        |
|---------------|-------|---------|------------------------------------|
| RedShift      | vec2  | 0,0     | Red channel displacement           |
| GreenShift    | vec2  | 0,0     | Green channel displacement         |
| BlueShift     | vec2  | 0,0     | Blue channel displacement          |
| RadialFalloff | float | 0.0     | Radial falloff from center         |
| StretchAngle  | float | 0.0     | Anamorphic stretch angle           |
| Mix           | float | 1.0     | Effect blend amount                |
