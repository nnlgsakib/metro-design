# metro-colorspace

Color space and gamut mapping plugin. Supports sRGB, Rec.709, Rec.2020, ACEScct, LogC, S-Log3, S-Log2, and V-Log with configurable gamut mapping modes.

## Parameters

| Parameter      | Type   | Default | Description                      |
|----------------|--------|---------|----------------------------------|
| InputSpace     | enum   | sRGB    | Source color space               |
| OutputSpace    | enum   | Rec.709 | Target color space               |
| GamutMapping   | enum   | None    | Out-of-gamut handling strategy   |
| ToneMap        | bool   | false   | Enable tone mapping              |
| ExposureAdjust | float  | 0.0     | Exposure compensation (stops)    |
| Mix            | float  | 1.0     | Effect blend amount              |
