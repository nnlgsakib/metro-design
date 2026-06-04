"""Generate reference .cube LUTs for the color grading smoke test suite.

Creates:
  - reference_luts/identity_33.cube       — 33x33x33 identity LUT
  - reference_luts/srgb_to_linear_33.cube — sRGB encoding → linear
  - reference_luts/linear_to_srgb_33.cube — linear → sRGB encoding
  - reference_luts/logc_to_linear_33.cube — ARRI LogC v3 → linear
  - reference_luts/linear_to_logc_33.cube — linear → ARRI LogC v3

The LogC pair use matched domains: LogC [0,1] ↔ linear [0, max_linear].
"""

import numpy as np
from colour.io import write_LUT
from colour import LUT3D

_DOMAIN_01 = np.array([[0, 0, 0], [1, 1, 1]], dtype=np.float64)


def _srgb_eotf(v):
    mask = v <= 0.04045
    out = np.empty_like(v)
    out[mask] = v[mask] / 12.92
    out[~mask] = np.power((v[~mask] + 0.055) / 1.055, 2.4)
    return out


def _srgb_oetf(v):
    mask = v <= 0.0031308
    out = np.empty_like(v)
    out[mask] = v[mask] * 12.92
    out[~mask] = 1.055 * np.power(v[~mask], 1.0 / 2.4) - 0.055
    return out


def _logc_eotf(v):
    cut = 0.010591
    a, b, c, d, e, f = 5.555556, 0.052272, 0.247190, 0.385537, 5.367655, 0.092809
    cut_enc = e * cut + f
    out = np.empty_like(v)
    out[v > cut_enc] = (np.power(10.0, (v[v > cut_enc] - d) / c) - b) / a
    out[v <= cut_enc] = (v[v <= cut_enc] - f) / e
    return np.clip(out, 0.0, None)


def _logc_oetf(v):
    cut = 0.010591
    a, b, c, d, e, f = 5.555556, 0.052272, 0.247190, 0.385537, 5.367655, 0.092809
    out = np.empty_like(v)
    out[v > cut] = c * np.log10(a * v[v > cut] + b) + d
    out[v <= cut] = e * v[v <= cut] + f
    return out


def _make_3d_lut(func, size=33, domain=_DOMAIN_01, name="lut"):
    n = size
    rgb = np.mgrid[0:n, 0:n, 0:n].astype(np.float64) / (n - 1)
    rgb = rgb.reshape(3, -1).T
    dmin = domain[0]
    drange = domain[1] - domain[0]
    rgb_scaled = rgb * drange + dmin
    table = func(rgb_scaled).reshape(n, n, n, 3)
    return LUT3D(table=table, name=name, size=(n, n, n), domain=domain)


def main():
    import os

    out_dir = os.path.join(os.path.dirname(__file__), "reference_luts")
    os.makedirs(out_dir, exist_ok=True)

    # --- LogC pair with matched domains ---
    # logc_to_linear: LogC [0,1] → linear
    max_lin = float(_logc_eotf(np.array([[1.0]]))[0, 0])
    lin_domain = np.array([[0, 0, 0], [max_lin, max_lin, max_lin]], dtype=np.float64)

    pairs = [
        ("identity_33.cube", lambda x: x, _DOMAIN_01, "identity"),
        ("srgb_to_linear_33.cube", _srgb_eotf, _DOMAIN_01, "srgb_to_linear"),
        ("linear_to_srgb_33.cube", _srgb_oetf, _DOMAIN_01, "linear_to_srgb"),
        ("logc_to_linear_33.cube", _logc_eotf, _DOMAIN_01, "logc_to_linear"),
        ("linear_to_logc_33.cube", _logc_oetf, lin_domain, "linear_to_logc"),
    ]

    for fname, func, domain, name in pairs:
        lut = _make_3d_lut(func, size=33, domain=domain, name=name)
        lut.comments = [name]
        path = os.path.join(out_dir, fname)
        write_LUT(lut, path)
        print(f"  Wrote {path}")


if __name__ == "__main__":
    main()
