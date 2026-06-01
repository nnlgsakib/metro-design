# Metropolis

Monorepo for Metro Design's DaVinci Resolve plugin suite — color grading, VFX, AI assistants, motion graphics, and workflow automation.

## Directory Structure

```
metropolis/
├── color-engine/     # Color science core (C++20, OpenFX)
├── vfx-engine/       # Visual effects & compositing (C++20, OpenFX)
├── ai-engine/        # ML inference & assistants (Python, ONNX Runtime)
├── motion/           # Motion graphics packs (C++20, Fusion templates)
├── workflow/         # Automation & batch processing (Python)
├── platform/         # Shared infrastructure (C++20)
├── backend/          # Cloud services (Go)
└── frontend/         # Website & marketplace (Next.js)
```

## Prerequisites

- **C++ modules**: CMake >= 3.20, C++20 compiler (GCC 12+, Clang 16+, MSVC 2022+)
- **Python modules**: Python >= 3.11, pip
- **Go backend**: Go >= 1.22

## Building

### C++ modules

```bash
cd metropolis
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Python packages

```bash
# AI bridge
pip install -e ai-engine/resolve-bridge

# Workflow scripts
pip install -e workflow/scripts
```

## Code Style

| Language | Formatter | Config |
|----------|-----------|--------|
| C++      | clang-format | `.clang-format` (Google style) |
| Python   | Black + Ruff | `pyproject.toml` |
| Go       | gofmt       | Go default |

Run all formatters before committing:

```bash
# C++
find . -name '*.cpp' -o -name '*.h' | xargs clang-format -i

# Python
black .
ruff check --fix .

# Go
gofmt -l -w .
```

## License

Proprietary. See LICENSE file.
