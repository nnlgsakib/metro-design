#!/usr/bin/env bash
# run-on-gpu.sh — Build and run CUDA verification tests on an NVIDIA GPU machine.
#
# Usage:
#   ./run-on-gpu.sh              # Build and run CUDA verification
#   ./run-on-gpu.sh --quick      # Run only (skip rebuild)
#
# Requirements:
#   - NVIDIA GPU with compute capability >= 5.2
#   - CUDA Toolkit >= 11.0
#   - CMake >= 3.18
#   - g++ with C++17 support

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-gpu-verify"
BIN_DIR="${BUILD_DIR}/bin"

echo "=============================================="
echo " MetroEffects — CUDA GPU Verification"
echo "=============================================="
echo ""

if ! command -v nvidia-smi &>/dev/null; then
    echo "ERROR: nvidia-smi not found. No NVIDIA GPU available."
    exit 1
fi
echo "GPU detected:"
nvidia-smi --query-gpu=name,compute_cap,memory.total --format=csv,noheader
echo ""

if ! command -v nvcc &>/dev/null; then
    echo "ERROR: nvcc not found. Install CUDA Toolkit >= 11.0."
    exit 1
fi
echo "NVCC: $(nvcc --version | tail -1)"
echo ""

if [ "${1:-}" = "--quick" ]; then
    if [ -f "${BIN_DIR}/cuda-verification" ]; then
        "${BIN_DIR}/cuda-verification"
        exit $?
    fi
    echo "No existing binary, rebuilding..."
fi

echo "=== Configuring ==="
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
    -DMETRO_USE_CUDA=ON \
    -DMETRO_BUILD_TESTS=ON \
    -DMETRO_BUILD_PLUGINS=OFF \
    -DCMAKE_BUILD_TYPE=Release

echo ""
echo "=== Building ==="
cmake --build "${BUILD_DIR}" --target cuda-verification -j"$(nproc)"

echo ""
echo "=============================================="
echo " Running CUDA Verification Tests"
echo "=============================================="
echo ""
cd "${BUILD_DIR}" && ctest --tests-regex cuda-verification --output-on-failure -V

echo ""
echo "=== Done ==="
