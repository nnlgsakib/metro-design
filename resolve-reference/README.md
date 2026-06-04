# DaVinci Resolve Reference Environment

Version-controlled reference environment bundle for DaVinci Resolve color
grading. This document captures reproducible project setup steps — no binary
`.drp` files are checked in.

## Table of Contents

- [Color Management Presets](#color-management-presets)
  - [ACEScct](#1-acescct-recommended-for-vfx-heavy-pipelines)
  - [DaVinci YRGB Color Managed](#2-davinci-yrgb-color-managed-recommended-for-delivery-fast-turns)
  - [Custom Color Management (No Preset)](#3-custom-color-management-no-preset)
- [Display Calibration Baseline](#display-calibration-baseline)
- [Project Format Presets](#project-format-presets)
- [Render Settings](#render-settings)

---

## Color Management Presets

All presets assume **Project Settings > Color Management** as the starting
point.

### 1. ACEScct (recommended for VFX-heavy pipelines)

| Setting                     | Value                                   |
| --------------------------- | --------------------------------------- |
| Color science               | ACEScct                                 |
| ACES version                | 1.3                                     |
| ACES input transform        | IDT per camera source (see notes below) |
| ACES output transform       | RRT + sRGB / Rec.709 (SDR)              |
|                              | RRT + Rec.2020 ST-2084 (HDR)            |
| ACES look transform         | None (set per node or via CTL)          |
| Gamut compression mode      | Luminance + hue mapping                 |
| Reference Gamut             | ACES AP1 (working) / ACES AP0 (scene)   |

**Notes**

- Choose IDT by camera: ARRI Alexa = ARRI ALEXA Wide Gamut / LogC3,
  RED = RED Wide Gamut / Log3G10, Sony = S-Gamut3 / S-Log3, Blackmagic =
  Blackmagic Design Film Gen 5.
- Enable **Use white point adaptation** when mixing cameras with different
  white points.
- For VFX plates, export with ACES 1.3 output transform and deliver OpenEXR
  in AP1.

### 2. DaVinci YRGB Color Managed (recommended for delivery-fast turns)

| Setting                     | Value                                    |
| --------------------------- | ---------------------------------------- |
| Color science               | DaVinci YRGB Color Managed               |
| Auto color management       | HDR or SDR (per timeline preference)     |
| SDR mastering reference     | Rec.709 / 2.4 gamma / 100 cd/m²         |
| HDR mastering reference     | Rec.2020 / ST-2084 / 1000 cd/m²          |
| Input color space           | Same IDT logic as ACES (per camera)      |
| Timeline color space        | Rec.709 / Rec.2020 depending on delivery |
| Output color space          | Same as timeline (passthrough)           |

**Notes**

- Use **SDR** for broadcast/web deliverables, **HDR** for theatrical / OTT
  HDR masters.
- Enable **Use separate color space for input and timeline** when mixing SDR
  and HDR sources in one project.

### 3. Custom Color Management (no preset)

For colorists managing every transform manually per node.

| Setting                     | Value                                     |
| --------------------------- | ----------------------------------------- |
| Color science               | DaVinci YRGB                               |
| Auto color management       | Disabled                                  |
| Input color space           | Timeline (no CST applied globally)        |
| Timeline color space        | Rec.709 Gamma 2.4 (or chosen working space) |
| Output color space          | Same as timeline                          |

**Notes**

- Every CST, CDL, or LUT transform is applied explicitly in the node graph.
- Recommended working spaces: Rec.709 Gamma 2.4 (SDR), Rec.2020 ST-2084
  (HDR), or ACES AP1 via manually placed CST nodes.
- Use a **reference group node stack** at the clip or timeline level to
  enforce the transform chain consistently.

---

## Display Calibration Baseline

Minimum requirements for a reference-grade color grading monitoring chain.

### Hardware requirements

| Item               | Minimum specification                                      |
| ------------------ | ---------------------------------------------------------- |
| Monitor            | 10-bit panel, 99% Rec.709 / 95% DCI-P3, ΔE ≤ 2            |
|                   | HDR: 10-bit, 1000 cd/m² peak, 99% Rec.2020, ΔE ≤ 2        |
| Interface          | SDI (Blackmagic DeckLink / UltraStudio) — not HDMI         |
| Probe              | Colorimeter (X-Rite i1 Display Pro or equivalent)          |
| Software           | CalMAN / ColourSpace / DisplayCAL for calibration & verify |

### Calibration workflow

1. Warm up monitor for **≥ 30 minutes** before profiling.
2. Set monitor to native gamut, manual white point D65.
3. Target SDR: **120 cd/m²** (dim surround), **100 cd/m²** (dark surround)
   at D65, Rec.709 / Gamma 2.4.
4. Target HDR: **203 cd/m²** at 50 % stimulus (ST-2084 electro-optical
   transfer function), Rec.2020 primaries, D65 white point.
5. Verify with at least 17-step grayscale sweep — ΔE < 3 for all steps,
   ΔE < 2 preferred.
6. Recalibrate **every 4 weeks** or after 500 hours of use, whichever comes
   first.

### Room environment

| Parameter         | Requirement                     |
| ----------------- | ------------------------------- |
| Ambient lighting  | D65 fluorescent / LED, dimmable |
| Wall colour       | Neutral grey (N5–N7)            |
| Background bias   | 6500 K, 10 cd/m²                |
| No direct glare   | On grading monitor surface      |

---

## Project Format Presets

Configure via **Project Settings > Master Settings > Timeline Format**.

### UHD (3840 × 2160)

| Setting                | Value                   |
| ---------------------- | ----------------------- |
| Resolution             | 3840 × 2160 Ultra HD    |
| Frame rate             | 23.976 / 24 / 25 / 50   |
| Pixel aspect ratio     | Square                  |
| Field dominance        | Progressive (no fields) |

### DCI 4K (4096 × 2160)

| Setting                | Value                   |
| ---------------------- | ----------------------- |
| Resolution             | 4096 × 2160 DCI 4K      |
| Frame rate             | 24 / 48                 |
| Pixel aspect ratio     | Square                  |
| Field dominance        | Progressive (no fields) |

### HD (1920 × 1080)

| Setting                | Value                   |
| ---------------------- | ----------------------- |
| Resolution             | 1920 × 1080 HD          |
| Frame rate             | 23.976 / 25 / 29.97 / 50 / 59.94 |
| Pixel aspect ratio     | Square                  |
| Field dominance        | Progressive (no fields) |

---

## Render Settings

One preset per delivery target. Configure via **Deliver** page, add to **Render
Settings** presets.

### Broadcast / Web (Rec.709 SDR)

| Setting               | Value                             |
| --------------------- | --------------------------------- |
| Format                | QuickTime                         |
| Codec                 | H.264                             |
| Resolution            | Match timeline                    |
| Frame rate            | Match timeline                    |
| Quality               | Restrict to 50 Mb/s               |
| Key frames            | Auto (every 1 second)             |
| Colour space tag      | Rec.709                           |
| Gamma tag             | Rec.709                            |
| Audio                 | AAC 320 kb/s, 48 kHz, stereo      |
| Output LUT            | None (trust timeline)             |

### Cinema / DCP (DCI P3 D65)

| Setting               | Value                             |
| --------------------- | --------------------------------- |
| Format                | QuickTime ProRes or DPX (10-bit)  |
| Codec                 | ProRes 4444 XQ / DPX 10-bit       |
| Resolution            | Match timeline                    |
| Frame rate            | Match timeline                    |
| Quality               | ProRes 4444 XQ                    |
| Colour space tag      | Rec.709 (DCI P3 on ingest)        |
| Gamma tag             | Gamma 2.6                         |
| Audio                 | PCM 48 kHz, 24-bit, 5.1 or stereo |
| Output LUT            | If ACES: RRT + P3-D65             |

### OTT / HDR (Rec.2020 ST-2084)

| Setting               | Value                             |
| --------------------- | --------------------------------- |
| Format                | QuickTime                         |
| Codec                 | H.265 (HEVC) / ProRes 4444        |
| Resolution            | Match timeline                    |
| Frame rate            | Match timeline                    |
| Quality               | H.265 Main 10 Profile, 100 Mb/s   |
|                        | ProRes 4444 for master            |
| Colour space tag      | Rec.2020                          |
| Gamma tag             | ST-2084                           |
| HDR metadata          | SMPTE ST 2086 / MaxFALL / MaxCLL  |
| Audio                 | AAC 320 kb/s, 48 kHz, stereo      |
| Output LUT            | None (trust timeline CST)         |
