#!/usr/bin/env python3
"""Scaffold a new Metro Design OFX plugin.

Usage:
    python3 scripts/metro-plugin-init.py init <plugin-name> [options]

Examples:
    python3 scripts/metro-plugin-init.py init my-effect
    python3 scripts/metro-plugin-init.py init blur --cuda --description "Gaussian blur effect"
    python3 scripts/metro-plugin-init.py init my-effect --dry-run
"""

import argparse
import os
import re
import sys
import textwrap

REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
PLUGINS_DIR = os.path.join(REPO_ROOT, "plugins")
ROOT_CMAKE = os.path.join(REPO_ROOT, "CMakeLists.txt")
VERIFIER = os.path.join(REPO_ROOT, "scripts", "verify-installer.py")


def kebab_to_pascal(name: str) -> str:
    return "".join(word.capitalize() for word in name.split("-"))


def kebab_to_camel(name: str) -> str:
    parts = name.split("-")
    return parts[0] + "".join(w.capitalize() for w in parts[1:])


def kebab_to_flat(name: str) -> str:
    return name.replace("-", "")


def kebab_to_label(name: str) -> str:
    return " ".join(word.capitalize() for word in name.split("-"))


def validate_plugin_name(name: str) -> None:
    if not re.match(r"^[a-z][a-z0-9-]*$", name):
        sys.exit(
            f"ERROR: plugin name must be kebab-case (lowercase letters, digits, hyphens), got '{name}'"
        )
    if name.startswith("metro-"):
        sys.exit(
            f"ERROR: plugin name should not include the 'metro-' prefix (it is added automatically), got '{name}'"
        )


def make_source(name: str, description: str, grouping: str) -> str:
    pascal = kebab_to_pascal(name)
    camel = kebab_to_camel(name)
    flat = kebab_to_flat(name)
    label = kebab_to_label(name)

    return textwrap.dedent(f"""\
    #include "metro/ofx/Plugin.hpp"
    #include "metro/ofx/param/ParamManager.hpp"
    #include <cstring>
    #include <cstdio>

    using namespace metro::ofx;
    using namespace metro::ofx::param;

    class {pascal}Plugin : public Plugin {{
    public:
        const char *identifier() const override {{ return "com.metrodesign.{flat}"; }}
        const char *label() const override {{ return "Metro {label}"; }}
        const char *description() const override {{
            return "{description}";
        }}
        const char *versionString() const override {{ return "1.0.0"; }}
        const char *pluginGrouping() const override {{ return "{grouping}"; }}

        OfxStatus describe(OfxImageEffectHandle descriptor) override
        {{
            if (!hostAvailable() || !host().properties()) return kOfxStatErrBadHandle;

            OfxPropertySetHandle props;
            OfxStatus stat = host().imageEffect()->getPropertySet(descriptor, &props);
            if (stat != kOfxStatOK) return stat;

            const OfxPropertySuiteV1 *prop = host().properties();

            prop->propSetString(props, kOfxImageEffectPropLabel, 0, label());
            prop->propSetString(props, kOfxImageEffectPropShortLabel, 0, "Metro{pascal}");
            prop->propSetString(props, kOfxImageEffectPropLongLabel, 0, "Metro Design {label}");
            prop->propSetString(props, kOfxImageEffectPropGrouping, 0, pluginGrouping());
            prop->propSetString(props, kOfxImageEffectPropDescription, 0, description());

            const char *contexts[] = {{ kOfxImageEffectContextFilter, kOfxImageEffectContextGeneral, nullptr }};
            prop->propSetStringN(props, kOfxImageEffectPropSupportedContexts, 2, contexts);

            return kOfxStatOK;
        }}

        OfxStatus describeInContext(OfxImageEffectHandle descriptor, int contextIndex) override
        {{
            (void)contextIndex;
            if (!hostAvailable() || !host().imageEffect() || !host().parameters())
                return kOfxStatErrBadHandle;

            OfxParamSetHandle paramSet;
            OfxStatus stat = host().imageEffect()->getParamSet(descriptor, &paramSet);
            if (stat != kOfxStatOK) return stat;

            OfxPropertySetHandle paramProps;
            const OfxParamSuiteV1 *param = host().parameters();

            stat = param->paramDefine(paramSet, kOfxParamTypeDouble, "gain", &paramProps);
            if (stat != kOfxStatOK) return stat;

            if (host().properties()) {{
                auto *prop = host().properties();
                prop->propSetString(paramProps, kOfxParamPropLabel, 0, "Gain");
                prop->propSetString(paramProps, kOfxParamPropHint, 0, "Multiplicative gain factor");
                prop->propSetDouble(paramProps, kOfxParamPropDoubleMin, 0, 0.0);
                prop->propSetDouble(paramProps, kOfxParamPropDoubleMax, 0, 10.0);
                prop->propSetDouble(paramProps, kOfxParamPropDoubleDefault, 0, 1.0);
                prop->propSetDouble(paramProps, kOfxParamPropIncrement, 0, 0.1);
                prop->propSetInt(paramProps, kOfxParamPropDigits, 0, 3);
            }}

            return kOfxStatOK;
        }}

        OfxStatus render(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) override
        {{
            (void)instance;
            (void)inArgs;
            (void)outArgs;

            if (!hostAvailable()) return kOfxStatErrBadHandle;

            auto info = host().probeCapabilities();
            std::printf("[Metro{pascal}] Rendering frame in host: %s v%s\\n",
                        info.name.c_str(), info.versionString.c_str());

            return kOfxStatOK;
        }}
    }};

    static {pascal}Plugin s_{camel}Plugin;
    static PluginRegistrar s_registrar(&s_{camel}Plugin);
    """)


def make_cmakelists_cpu(name: str) -> str:
    pascal = kebab_to_pascal(name)
    return textwrap.dedent(f"""\
    set(SOURCES
        src/{pascal}Plugin.cpp
    )

    metro_add_plugin(metro-{name}
        SOURCES ${{SOURCES}}
        LINK_LIBS ofx-core
    )

    target_include_directories(metro-{name}
        PRIVATE
            ${{CMAKE_SOURCE_DIR}}/third_party/openfx/include
            ${{CMAKE_SOURCE_DIR}}/libs/ofx-core/include
    )
    """)


def make_cmakelists_cuda(name: str) -> str:
    pascal = kebab_to_pascal(name)
    return textwrap.dedent(f"""\
    set(SOURCES
        src/{pascal}Plugin.cpp
    )

    if(METRO_HAVE_CUDA)
        list(APPEND SOURCES src/{pascal}Kernels.cu)
    endif()

    metro_add_plugin(metro-{name}
        SOURCES ${{SOURCES}}
        LINK_LIBS ofx-core ofx-gpu
    )

    metro_target_cuda(metro-{name})

    target_include_directories(metro-{name}
        PRIVATE
            ${{CMAKE_SOURCE_DIR}}/third_party/openfx/include
            ${{CMAKE_SOURCE_DIR}}/libs/ofx-core/include
            ${{CMAKE_SOURCE_DIR}}/libs/ofx-gpu/include
    )
    """)


def make_cuda_kernel_header(name: str) -> str:
    camel = kebab_to_camel(name)
    return textwrap.dedent(f"""\
    #pragma once

    void launch_{camel}Kernel(
        float* output, const float* input, int width, int height, float intensity);
    """)


def make_cuda_kernel_source(name: str) -> str:
    pascal = kebab_to_pascal(name)
    camel = kebab_to_camel(name)
    return textwrap.dedent(f"""\
    extern "C" __global__ void {camel}_kernel(
        float* output, const float* input, int width, int height, float intensity)
    {{
        int x = blockIdx.x * blockDim.x + threadIdx.x;
        int y = blockIdx.y * blockDim.y + threadIdx.y;
        if (x >= width || y >= height) return;

        int idx = y * width + x;
        output[idx] = input[idx] * intensity;
    }}

    void launch_{camel}Kernel(
        float* output, const float* input, int width, int height, float intensity)
    {{
        dim3 block(16, 16);
        dim3 grid((width + 15) / 16, (height + 15) / 16);
        {camel}_kernel<<<grid, block>>>(output, input, width, height, intensity);
    }}
    """)


def add_to_root_cmake(name: str, dry_run: bool = False) -> None:
    line = f"add_subdirectory(plugins/metro-{name})"
    marker = "if(METRO_BUILD_PLUGINS)"
    insert_after = None

    with open(ROOT_CMAKE) as f:
        lines = f.readlines()

    for i, l in enumerate(lines):
        if marker in l:
            insert_after = i
        elif (
            l.strip().startswith("add_subdirectory(plugins/")
            and insert_after is not None
        ):
            insert_after = i

    if insert_after is None:
        print(
            "  WARNING: could not find METRO_BUILD_PLUGINS block in root CMakeLists.txt"
        )
        return

    existing = {
        l.strip() for l in lines if l.strip().startswith("add_subdirectory(plugins/")
    }
    if f"add_subdirectory(plugins/metro-{name})" in existing:
        print(f"  Already registered in root CMakeLists.txt (skipping)")
        return

    indent = "    "
    new_line = f"{indent}{line}\n"
    lines.insert(insert_after + 1, new_line)

    if dry_run:
        print(f"  Would add to {ROOT_CMAKE}: {line}")
        return

    with open(ROOT_CMAKE, "w") as f:
        f.writelines(lines)
    print(f"  Registered in root CMakeLists.txt")


def cmd_init(args: argparse.Namespace) -> None:
    validate_plugin_name(args.name)

    dir_name = f"metro-{args.name}"
    plugin_dir = os.path.join(PLUGINS_DIR, dir_name)
    src_dir = os.path.join(plugin_dir, "src")

    if os.path.exists(plugin_dir) and not args.force:
        sys.exit(
            f"ERROR: directory already exists: {plugin_dir}\n  Use --force to overwrite"
        )
    if os.path.exists(plugin_dir) and args.force:
        print(f"  Overwriting existing directory: {plugin_dir}")

    description = args.description or f"Metro Design {kebab_to_label(args.name)} plugin"

    cmake_content = (
        make_cmakelists_cuda(args.name) if args.cuda else make_cmakelists_cpu(args.name)
    )
    source_content = make_source(args.name, description, args.grouping)
    source_file = f"{kebab_to_pascal(args.name)}Plugin.cpp"

    print(f"\nScaffolding plugin: {dir_name}")
    print(f"  Directory:   plugins/{dir_name}/")
    print(f"  Class:       {kebab_to_pascal(args.name)}Plugin")
    print(f"  Identifier:  com.metrodesign.{kebab_to_flat(args.name)}")
    print(f"  Label:       Metro {kebab_to_label(args.name)}")
    print(f"  Description: {description}")
    print(f"  Grouping:    {args.grouping}")
    if args.cuda:
        print(f"  CUDA:        yes")
    print()

    if args.dry_run:
        print("  [DRY RUN] Would create:")
        print(f"    plugins/{dir_name}/CMakeLists.txt")
        print(f"    plugins/{dir_name}/src/{source_file}")
        if args.cuda:
            print(f"    plugins/{dir_name}/src/{kebab_to_pascal(args.name)}Kernels.cuh")
            print(f"    plugins/{dir_name}/src/{kebab_to_pascal(args.name)}Kernels.cu")
        add_to_root_cmake(args.name, dry_run=True)
        return

    os.makedirs(src_dir, exist_ok=True)

    cmake_path = os.path.join(plugin_dir, "CMakeLists.txt")
    with open(cmake_path, "w") as f:
        f.write(cmake_content)
    print(f"  Created: {os.path.relpath(cmake_path, REPO_ROOT)}")

    src_path = os.path.join(src_dir, source_file)
    with open(src_path, "w") as f:
        f.write(source_content)
    print(f"  Created: {os.path.relpath(src_path, REPO_ROOT)}")

    if args.cuda:
        kernel_hdr = os.path.join(src_dir, f"{kebab_to_pascal(args.name)}Kernels.cuh")
        with open(kernel_hdr, "w") as f:
            f.write(make_cuda_kernel_header(args.name))
        print(f"  Created: {os.path.relpath(kernel_hdr, REPO_ROOT)}")

        kernel_src = os.path.join(src_dir, f"{kebab_to_pascal(args.name)}Kernels.cu")
        with open(kernel_src, "w") as f:
            f.write(make_cuda_kernel_source(args.name))
        print(f"  Created: {os.path.relpath(kernel_src, REPO_ROOT)}")

    if not args.no_register:
        add_to_root_cmake(args.name)

    print(f"\nDone. Next steps:")
    print(f"  1. cmake -B build -DMETRO_BUILD_PLUGINS=ON")
    print(f"  2. cmake --build build --parallel")
    print(f"  3. ctest --test-dir build")
    print(f"  4. Edit plugins/{dir_name}/src/{source_file} to implement your effect")


def cmd_list(args: argparse.Namespace) -> None:
    if not os.path.isdir(PLUGINS_DIR):
        print("No plugins directory found.")
        return

    plugins = sorted(
        d
        for d in os.listdir(PLUGINS_DIR)
        if os.path.isdir(os.path.join(PLUGINS_DIR, d)) and d.startswith("metro-")
    )

    if not plugins:
        print("No plugins found.")
        return

    print(f"Plugins ({len(plugins)}):")
    for p in plugins:
        cmake = os.path.join(PLUGINS_DIR, p, "CMakeLists.txt")
        has_cuda = False
        if os.path.isfile(cmake):
            with open(cmake) as f:
                has_cuda = "METRO_HAVE_CUDA" in f.read()
        suffix = " [CUDA]" if has_cuda else ""
        print(f"  {p}{suffix}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Metro Design plugin scaffolding CLI",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""\
            Examples:
              python3 scripts/metro-plugin-init.py init my-effect
              python3 scripts/metro-plugin-init.py init blur --cuda --description "Gaussian blur"
              python3 scripts/metro-plugin-init.py init my-effect --dry-run
              python3 scripts/metro-plugin-init.py list
        """),
    )
    sub = parser.add_subparsers(dest="command", required=True)

    init_parser = sub.add_parser("init", help="Scaffold a new plugin")
    init_parser.add_argument("name", help="Plugin name in kebab-case (e.g. my-effect)")
    init_parser.add_argument(
        "--cuda", action="store_true", help="Include CUDA kernel scaffolding"
    )
    init_parser.add_argument("--description", help="Plugin description")
    init_parser.add_argument(
        "--grouping",
        default="Metro Design",
        help="Plugin grouping in host UI (default: Metro Design)",
    )
    init_parser.add_argument(
        "--dry-run", action="store_true", help="Preview without creating files"
    )
    init_parser.add_argument(
        "--force", action="store_true", help="Overwrite existing plugin directory"
    )
    init_parser.add_argument(
        "--no-register", action="store_true", help="Skip adding to root CMakeLists.txt"
    )

    sub.add_parser("list", help="List existing plugins")

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()

    if args.command == "init":
        cmd_init(args)
    elif args.command == "list":
        cmd_list(args)


if __name__ == "__main__":
    main()
