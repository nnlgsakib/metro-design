# metro-sample

Reference OFX plugin for Metro Design. Demonstrates the standard plugin structure, parameter model, and rendering pipeline.

## Overview

metro-sample is a minimal working plugin that serves as a template for all new Metro Design OFX plugins. It implements a basic color pass-through with a single adjustable parameter.

## Parameters

| Parameter | Type  | Default | Description                        |
|-----------|-------|---------|------------------------------------|
| Opacity   | float | 1.0     | Output opacity (0.0 = transparent) |

## Usage

Apply in DaVinci Resolve from the OpenFX panel under "Metro → Sample".

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DMETRO_BUILD_PLUGINS=ON
cmake --build build --parallel
```

The plugin binary is installed to `build/Plugins/`.

## Source layout

```
plugins/metro-sample/
├── CMakeLists.txt
└── src/
    ├── PluginEntry.cpp       # OFX plugin entry point
    ├── PluginFactory.cpp     # Plugin factory
    └── PluginFactory.h       # Plugin factory declaration
```

## Dependencies

- OpenFX SDK (`third_party/openfx/`)
- ofx-core (`libs/ofx-core/`)
