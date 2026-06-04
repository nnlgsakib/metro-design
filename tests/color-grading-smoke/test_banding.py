"""Banding artifact detection on 8-bit vs 10-bit test patterns.

Validates that quantized gradients through the LUT show visible
banding steps at 8-bit but maintain smoothness at 10-bit depth.
"""

import numpy as np
from conftest import apply_lut


def _quantize(rgb: np.ndarray, bit_depth: int) -> np.ndarray:
    """Quantize floating [0,1] RGB to integer bit depth and back."""
    levels = (1 << bit_depth) - 1
    return np.round(rgb * levels) / levels


def _count_repeated_steps(gradient: np.ndarray) -> int:
    """Count number of times consecutive gradient values round to the same quantized level."""
    luma = 0.2126 * gradient[:, 0] + 0.7152 * gradient[:, 1] + 0.0722 * gradient[:, 2]
    q = np.round(luma * 255) / 255
    repeats = np.sum(np.abs(np.diff(q)) < 1e-6)
    return repeats


def test_8bit_banding_detected(srgb_to_linear_lut):
    """8-bit gradient through sRGB→linear LUT should show banding."""
    ramp = np.linspace(0.0, 1.0, 256)
    rgb = np.column_stack([ramp, ramp, ramp])
    quantized_8 = _quantize(rgb, 8)
    result = apply_lut(srgb_to_linear_lut, quantized_8)
    repeats = _count_repeated_steps(result)
    assert repeats > 20, f"8-bit banding: only {repeats} repeated steps (expected >20)"


def test_10bit_less_banding_than_8bit(srgb_to_linear_lut):
    """10-bit gradient should show significantly less banding than 8-bit."""
    ramp = np.linspace(0.0, 1.0, 1024)
    rgb = np.column_stack([ramp, ramp, ramp])

    q8 = _quantize(rgb, 8)
    q10 = _quantize(rgb, 10)

    result_8 = apply_lut(srgb_to_linear_lut, q8)
    result_10 = apply_lut(srgb_to_linear_lut, q10)

    repeats_8 = _count_repeated_steps(result_8)
    repeats_10 = _count_repeated_steps(result_10)

    assert repeats_10 < repeats_8, (
        f"10-bit ({repeats_10}) should have fewer repeats than 8-bit ({repeats_8})"
    )


def test_higher_bitdepth_reduces_banding(srgb_to_linear_lut):
    """Banding repeats decrease as bit depth increases."""
    ramp = np.linspace(0.0, 1.0, 1024)
    rgb = np.column_stack([ramp, ramp, ramp])

    q4 = _quantize(rgb, 4)
    q8 = _quantize(rgb, 8)
    q10 = _quantize(rgb, 10)

    r4 = _count_repeated_steps(apply_lut(srgb_to_linear_lut, q4))
    r8 = _count_repeated_steps(apply_lut(srgb_to_linear_lut, q8))
    r10 = _count_repeated_steps(apply_lut(srgb_to_linear_lut, q10))

    assert r4 >= r8 >= r10, (
        f"Banding not monotonic with bit depth: 4-bit={r4}, 8-bit={r8}, 10-bit={r10}"
    )


def test_banding_edge_case_black_white(srgb_to_linear_lut):
    """8-bit black-to-white ramp through LUT should show banding."""
    ramp = np.linspace(0.0, 1.0, 256)
    rgb = np.column_stack([ramp, ramp, ramp])
    q = _quantize(rgb, 8)
    result = apply_lut(srgb_to_linear_lut, q)
    repeats = _count_repeated_steps(result)
    assert repeats > 10, f"Black-to-white 8-bit: only {repeats} repeats"
