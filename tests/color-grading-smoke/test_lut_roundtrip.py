"""LUT round-trip validation: apply LUT → invert LUT → compare to original.

For low-resolution (33^3) LUTs with nonlinear transforms, some interpolation
error is expected. Tests verify that:
  - Identity LUT gives exact results for any input
  - Nonlinear round-trips stay within a practical dE00 budget
"""

import numpy as np
from conftest import apply_lut, compute_dE00


def test_identity_lut_roundtrip(identity_lut, default_test_colors):
    result = apply_lut(identity_lut, default_test_colors)
    dE = compute_dE00(default_test_colors, result)
    assert np.all(dE < 0.01), f"Identity LUT dE00 too large: max={np.max(dE)}"


def test_srgb_roundtrip(srgb_to_linear_lut, linear_to_srgb_lut, default_test_colors):
    linear = apply_lut(srgb_to_linear_lut, default_test_colors)
    recovered = apply_lut(linear_to_srgb_lut, linear)
    dE = compute_dE00(default_test_colors, recovered)
    max_de = np.max(dE)
    assert max_de < 3.0, f"sRGB round-trip dE00 too large: max={max_de:.2f}"


def test_logc_fwd_monotonic(logc_to_linear_lut):
    """LogC→linear must be monotonic: larger LogC → larger linear."""
    n = 33
    grid = np.mgrid[0:n, 0:n, 0:n].astype(np.float64) / (n - 1)
    grid = grid.reshape(3, -1).T
    linear = apply_lut(logc_to_linear_lut, grid)
    diffs = np.diff(linear[:, 0])
    assert np.all(diffs >= -1e-9), "LogC→linear not monotonic"


def test_black_point_preserved(identity_lut):
    black = np.array([[0.0, 0.0, 0.0]])
    result = apply_lut(identity_lut, black)
    assert np.max(np.abs(result - black)) < 1e-10, "Black point shifted"


def test_white_point_preserved(identity_lut):
    white = np.array([[1.0, 1.0, 1.0]])
    result = apply_lut(identity_lut, white)
    assert np.max(np.abs(result - white)) < 1e-10, "White point shifted"
