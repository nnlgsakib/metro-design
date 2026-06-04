"""Exposure consistency test against waveform target.

Validates that applying a LUT maintains consistent exposure
relationships across a range of input values.
"""

import numpy as np
from conftest import apply_lut


def _luminance(rgb: np.ndarray) -> np.ndarray:
    """Rec.709 luma from RGB."""
    return 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]


def test_exposure_monotonicity(srgb_to_linear_lut):
    """Exposure ramp through LUT must be monotonic: brighter in → brighter out."""
    ramp = np.linspace(0.0, 1.0, 50)
    ramp_rgb = np.column_stack([ramp, ramp, ramp])
    result = apply_lut(srgb_to_linear_lut, ramp_rgb)
    luma = _luminance(result)
    diffs = np.diff(luma)
    assert np.all(diffs >= -1e-9), (
        f"Non-monotonic exposure at indices {np.where(diffs < -1e-9)[0]}"
    )


def test_exposure_double_preserved(linear_to_srgb_lut):
    """A 1-stop exposure difference in linear should be ~1 stop in encoded-space."""
    midgray_lin = np.array([[0.18, 0.18, 0.18]])
    one_stop_lin = np.array([[0.36, 0.36, 0.36]])

    enc_mid = apply_lut(linear_to_srgb_lut, midgray_lin)
    enc_up = apply_lut(linear_to_srgb_lut, one_stop_lin)

    lum_mid = _luminance(enc_mid)[0]
    lum_up = _luminance(enc_up)[0]

    if lum_mid > 0 and lum_up > 0:
        encoded_ratio = lum_up / lum_mid
        assert encoded_ratio > 1.2, (
            f"Exposure ratio in encoded space is {encoded_ratio:.2f}x, expected >1.2x "
            f"(lum_mid={lum_mid:.4f}, lum_up={lum_up:.4f})"
        )


def test_black_remains_black(srgb_to_linear_lut):
    """Pure black input must produce black output."""
    black = np.array([[0.0, 0.0, 0.0]])
    result = apply_lut(srgb_to_linear_lut, black)
    assert np.max(np.abs(result)) < 1e-10, "Black not preserved through LUT"


def test_waveform_target_range(logc_to_linear_lut):
    """LogC mid-gray should map to expected linear range."""
    logc_midgray = np.array([[0.391, 0.391, 0.391]])
    result = apply_lut(logc_to_linear_lut, logc_midgray)
    luma = _luminance(result)[0]
    assert 0.15 <= luma <= 0.22, (
        f"LogC mid-gray luma {luma:.3f} outside expected range [0.15, 0.22]"
    )
