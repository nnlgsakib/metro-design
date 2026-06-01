# Metro Design

Monorepo for Metro Design's DaVinci Resolve plugin suite — color grading, VFX, AI assistants, motion graphics, and workflow automation.

## Structure

```
.
├── plugins/                # Metro Effects OFX plugin pack
│   ├── metro-ascii/        # ASCII art effect
│   ├── metro-blobtrack/    # Blob detection & tracking
│   ├── metro-chromab/      # Chromatic aberration
│   └── metro-sample/       # Reference plugin
├── metropolis/             # Full application suite
│   ├── color-engine/       # Color science core (C++20, OpenFX)
│   ├── vfx-engine/         # Visual effects & compositing (C++20, OpenFX)
│   ├── ai-engine/          # ML inference & assistants (Python, ONNX)
│   ├── motion/             # Motion graphics packs (C++20, Fusion)
│   ├── workflow/           # Automation & batch processing (Python)
│   ├── platform/           # Shared infrastructure (C++20)
│   ├── backend/            # Cloud services (Go)
│   └── frontend/           # Website & marketplace (Next.js)
├── libs/ofx-core/          # Shared OFX plugin framework
├── tests/                  # Plugin unit tests
├── third_party/openfx/     # OpenFX SDK
└── cmake/                  # Build configuration
```

## Prerequisites

- CMake >= 3.16 (plugins) / 3.20 (metropolis)
- C++17 (plugins) / C++20 (metropolis) compiler
- Python >= 3.11 (AI engine, workflow)
- Go >= 1.22 (backend)

## Building plugins

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DMETRO_BUILD_PLUGINS=ON
cmake --build build --parallel
ctest --test-dir build
cpack --config build/CPackConfig.cmake
```

## CI/CD

GitHub Actions workflows in `.github/workflows/`:
- `ci.yml` — Lint, build, test, and package on push/PR to `main`

## Code style

See `.clang-format` (C++), `pyproject.toml` (Python), and `CONTRIBUTING.md`.
