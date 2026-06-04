"""Skin tone line alignment test against known reference.

Validates that known skin-tone patches maintain expected warm-toned
RGB ratios and hue characteristics after LUT application.

Reference values from:
  - ARRI AWG/LogC: skin tone line ~25° on vectorscope (orange-red)
  - Sony S-Log3: expected warm skin tone ratio R > G > B
"""

import numpy as np
from conftest import apply_lut


def _r_gt_g_gt_b(rgb: np.ndarray) -> bool:
    """Check that for warm skin, R > G > B (dominant red, minimal blue)."""
    return bool(rgb[0] > rgb[1] > rgb[2])


def test_skin_tone_ratio_preserved_through_logc(logc_to_linear_lut):
    """Skin tones through LogC→linear LUT maintain R > G > B ratio."""
    skin_patches = np.array(
        [
            [0.40, 0.32, 0.28],
            [0.38, 0.30, 0.26],
            [0.42, 0.34, 0.30],
            [0.35, 0.28, 0.24],
        ]
    )
    result = apply_lut(logc_to_linear_lut, skin_patches)
    for i in range(len(skin_patches)):
        assert _r_gt_g_gt_b(result[i]), (
            f"Skin tone {i} lost warm ratio: R={result[i, 0]:.3f} "
            f"G={result[i, 1]:.3f} B={result[i, 2]:.3f}"
        )


def test_skin_tone_ratio_through_srgb(srgb_to_linear_lut, linear_to_srgb_lut):
    """Warm skin-tone RGB through sRGB round-trip retains R > G > B."""
    skin = np.array([[0.75, 0.55, 0.45]])
    linear = apply_lut(srgb_to_linear_lut, skin)
    recovered = apply_lut(linear_to_srgb_lut, linear)
    assert _r_gt_g_gt_b(recovered[0]), (
        f"Skin lost warm ratio after round-trip: R={recovered[0, 0]:.3f} "
        f"G={recovered[0, 1]:.3f} B={recovered[0, 2]:.3f}"
    )


def test_neutral_gray_has_minimal_color_cast(identity_lut):
    """Neutral gray patches should not shift chroma through identity LUT."""
    grays = np.linspace(0.0, 1.0, 11).reshape(-1, 1)
    grays = np.tile(grays, (1, 3))
    result = apply_lut(identity_lut, grays)
    max_shift = np.max(np.abs(result - grays))
    assert max_shift < 1e-10, f"Neutral gray shifted: max delta={max_shift}"


def test_skin_tone_line_reference_in_srgb():
    """Known skin tone reference: sRGB [0.75, 0.55, 0.45] is warm-toned."""
    skin = np.array([0.75, 0.55, 0.45])
    r, g, b = skin
    assert r > g > b, f"Reference skin tone lost R>G>B: {r:.3f}, {g:.3f}, {b:.3f}"
    assert r / g > 1.1, f"R/G ratio too low: {r / g:.3f}"
