"""Gamut clipping detection test.

Detects when out-of-gamut colors are incorrectly clipped or when
in-gamut colors have channels in unexpected ranges after LUT application.
"""

import numpy as np
from conftest import apply_lut


def _detect_clipped(result: np.ndarray, tolerance: float = 1e-6) -> np.ndarray:
    """Return mask of samples where any channel is clipped at 0 or 1."""
    return (result <= tolerance) | (result >= 1.0 - tolerance)


def test_in_gamut_not_clipped(srgb_to_linear_lut):
    """Saturated but valid sRGB colors should not be clipped."""
    colors = np.array(
        [
            [0.9, 0.1, 0.1],
            [0.1, 0.9, 0.1],
            [0.1, 0.1, 0.9],
            [0.8, 0.7, 0.2],
            [0.2, 0.8, 0.7],
        ]
    )
    result = apply_lut(srgb_to_linear_lut, colors)
    clipped = _detect_clipped(result)
    assert not np.any(clipped), (
        f"In-gamut colors clipped at indices {np.where(clipped)}"
    )


def test_out_of_gamut_handled_gracefully(logc_to_linear_lut):
    """Extreme out-of-gamut values should be clamped reasonably."""
    extreme = np.array(
        [
            [2.0, 0.0, 0.0],
            [0.0, 2.0, 0.0],
            [0.0, 0.0, 2.0],
            [-0.5, 0.0, 0.0],
        ]
    )
    result = apply_lut(logc_to_linear_lut, extreme)
    assert np.all(result >= 0.0), "Negative values in LUT output"
    assert np.any(result[0] >= 1.0) or np.any(result[0] > 0.99), (
        "Saturated channel not reaching maximum"
    )


def test_gamut_boundary_continuous(linear_to_srgb_lut):
    """At gamut boundary, small input changes should not cause large output jumps."""
    boundary = np.linspace(0.97, 1.0, 10)
    boundary_rgb = np.column_stack([boundary, boundary * 0.5, boundary * 0.25])
    result = apply_lut(linear_to_srgb_lut, boundary_rgb)
    diffs = np.diff(result, axis=0)
    max_step = np.max(np.abs(diffs))
    assert max_step < 0.05, (
        f"Discontinuity at gamut boundary: max step = {max_step:.4f}"
    )


def test_no_unexpected_negative(identity_lut, default_test_colors):
    """Valid input colors should not produce negative output."""
    result = apply_lut(identity_lut, default_test_colors)
    assert np.all(result >= -1e-10), "Negative values produced by LUT"
