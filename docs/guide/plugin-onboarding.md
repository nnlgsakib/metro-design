# Plugin Onboarding Guide

This guide walks through creating a new Metro Design OFX plugin.

## Prerequisites

- CMake >= 3.18
- C++17 compiler
- OpenFX SDK (included at `third_party/openfx/`)

## Step 1: Create the plugin directory

```
plugins/metro-<name>/
├── CMakeLists.txt
├── src/
│   ├── PluginEntry.cpp       # OFX plugin entry point
│   ├── PluginFactory.cpp     # Plugin factory implementation
│   └── PluginFactory.h       # Plugin factory header
└── docs/
    └── README.md             # Plugin documentation (also add to docs/SUMMARY.md)
```

## Step 2: Add CMake build

In `plugins/metro-<name>/CMakeLists.txt`:

```cmake
metro_add_plugin(
  NAME         metro-<name>
  SOURCES
    src/PluginEntry.cpp
    src/PluginFactory.cpp
    src/PluginFactory.h
)
```

Register the plugin in the root `CMakeLists.txt`:

```cmake
add_subdirectory(plugins/metro-<name>)
```

## Step 3: Write plugin documentation

Create `docs/plugins/metro-<name>/README.md` following this template:

```markdown
# metro-<name>

Brief one-line description of the plugin.

## Overview

What the plugin does, when to use it, and how it fits into a grading/VFX workflow.

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| Param1    | float | 0.5     | Controls X effect |
| Param2    | color | 1,1,1   | Tint color |

## Usage

```python
# DaVinci Resolve script example
resolve = bmd.scriptapp("Resolve")
```

## Building

```bash
cmake -B build -DMETRO_BUILD_PLUGINS=ON
cmake --build build --parallel
```

## Dependencies

- OpenFX
- ofx-core (shared framework)
```

Then add the new doc to `docs/SUMMARY.md`.

## Step 4: Add tests

See `CONTRIBUTING.md` for test conventions. Create tests in `tests/metro-<name>-test/`.

## Step 5: Submit a PR

Open a pull request against `main` with `feature/metro-<name>` branch name.

Your PR **must** include:
- Plugin source code
- Plugin documentation in `docs/plugins/metro-<name>/`
- Unit tests in `tests/metro-<name>-test/`
- Doxygen comments on all public API symbols
