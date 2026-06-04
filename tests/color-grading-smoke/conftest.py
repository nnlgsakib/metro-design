from pathlib import Path

import numpy as np
import pytest
from colour.io import read_LUT
from colour import LUT3D
from colour.difference import delta_E_CIE2000
from colour.models import XYZ_to_Lab
from colour import CCS_ILLUMINANTS

HERE = Path(__file__).parent
REF_LUTS = HERE / "reference_luts"
D65 = CCS_ILLUMINANTS["CIE 1931 2 Degree Standard Observer"]["D65"]

SRGB_TO_XYZ_MAT = np.array(
    [
        [0.412391, 0.357584, 0.180481],
        [0.212639, 0.715169, 0.072192],
        [0.019331, 0.119195, 0.950532],
    ]
)


@pytest.fixture(scope="session")
def ref_dir() -> Path:
    REF_LUTS.mkdir(parents=True, exist_ok=True)
    return REF_LUTS


@pytest.fixture(scope="session")
def identity_lut(ref_dir):
    return read_LUT(str(ref_dir / "identity_33.cube"))


@pytest.fixture(scope="session")
def srgb_to_linear_lut(ref_dir):
    return read_LUT(str(ref_dir / "srgb_to_linear_33.cube"))


@pytest.fixture(scope="session")
def linear_to_srgb_lut(ref_dir):
    return read_LUT(str(ref_dir / "linear_to_srgb_33.cube"))


@pytest.fixture(scope="session")
def logc_to_linear_lut(ref_dir):
    return read_LUT(str(ref_dir / "logc_to_linear_33.cube"))


@pytest.fixture(scope="session")
def linear_to_logc_lut(ref_dir):
    return read_LUT(str(ref_dir / "linear_to_logc_33.cube"))


@pytest.fixture(scope="session")
def default_test_colors() -> np.ndarray:
    return np.array(
        [
            [0.0, 0.0, 0.0],
            [0.18, 0.18, 0.18],
            [0.5, 0.5, 0.5],
            [1.0, 1.0, 1.0],
            [1.0, 0.0, 0.0],
            [0.0, 1.0, 0.0],
            [0.0, 0.0, 1.0],
            [0.5, 0.0, 0.0],
            [0.0, 0.5, 0.0],
            [0.0, 0.0, 0.5],
            [0.25, 0.5, 0.75],
        ],
        dtype=np.float64,
    )


def apply_lut(lut: LUT3D, rgb: np.ndarray) -> np.ndarray:
    size = int(lut.size)
    domain_min = lut.domain[0]
    domain_max = lut.domain[1]
    domain_range = domain_max - domain_min

    normalized = (rgb - domain_min) / domain_range
    normalized = np.clip(normalized, 0.0, 1.0)

    scaled = normalized * (size - 1)
    indices = scaled.astype(np.int64)
    fracs = scaled - indices.astype(np.float64)

    i0 = np.clip(indices, 0, size - 1)
    i1 = np.clip(indices + 1, 0, size - 1)

    def _gather(triplet):
        r, g, b = triplet[:, 0], triplet[:, 1], triplet[:, 2]
        return lut.table[r, g, b, :]

    c000 = _gather(np.stack([i0[:, 0], i0[:, 1], i0[:, 2]], axis=1))
    c100 = _gather(np.stack([i1[:, 0], i0[:, 1], i0[:, 2]], axis=1))
    c010 = _gather(np.stack([i0[:, 0], i1[:, 1], i0[:, 2]], axis=1))
    c110 = _gather(np.stack([i1[:, 0], i1[:, 1], i0[:, 2]], axis=1))
    c001 = _gather(np.stack([i0[:, 0], i0[:, 1], i1[:, 2]], axis=1))
    c101 = _gather(np.stack([i1[:, 0], i0[:, 1], i1[:, 2]], axis=1))
    c011 = _gather(np.stack([i0[:, 0], i1[:, 1], i1[:, 2]], axis=1))
    c111 = _gather(np.stack([i1[:, 0], i1[:, 1], i1[:, 2]], axis=1))

    fx = fracs[:, 0:1]
    fy = fracs[:, 1:2]
    fz = fracs[:, 2:3]

    c00 = c000 * (1 - fx) + c100 * fx
    c01 = c001 * (1 - fx) + c101 * fx
    c10 = c010 * (1 - fx) + c110 * fx
    c11 = c011 * (1 - fx) + c111 * fx
    c0 = c00 * (1 - fy) + c10 * fy
    c1 = c01 * (1 - fy) + c11 * fy
    return c0 * (1 - fz) + c1 * fz


def compute_dE00(reference: np.ndarray, sample: np.ndarray) -> np.ndarray:
    rgb_ref = np.asarray(reference, dtype=np.float64).reshape(-1, 3)
    rgb_smp = np.asarray(sample, dtype=np.float64).reshape(-1, 3)
    xyz_ref = rgb_ref @ SRGB_TO_XYZ_MAT.T
    xyz_smp = rgb_smp @ SRGB_TO_XYZ_MAT.T
    lab_ref = XYZ_to_Lab(xyz_ref, D65)
    lab_smp = XYZ_to_Lab(xyz_smp, D65)
    return delta_E_CIE2000(lab_ref, lab_smp)
