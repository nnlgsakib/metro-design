#!/usr/bin/env python3
"""Verify MetroEffects CPack installer artifacts.

Checks:
- All 11 plugins present in the archive
- Each .so/.dll/.bundle is a valid binary (not empty)
- ELF .so files export required OFX entry points
- README.md included
- No unexpected files

Usage:
    python3 verify-installer.py <path-to-archive>
"""

import os
import re
import subprocess
import sys
import tarfile
import tempfile
import shutil
import zipfile

EXPECTED_PLUGINS = [
    "metro-sample",
    "metro-ascii",
    "metro-blobtrack",
    "metro-chromab",
    "metro-vrteams",
    "metro-glow",
    "metro-filmgrain",
    "metro-lensflare",
    "metro-splittone",
    "metro-colorspace",
    "metro-transitions",
]

OFX_REQUIRED_SYMBOLS = ["OfxGetNumberOfPlugins", "OfxGetPlugin"]

EXPECTED_README = "README.md"


def is_valid_elf(path):
    with open(path, "rb") as f:
        magic = f.read(4)
    return magic == b"\x7fELF"


def is_valid_pe(path):
    with open(path, "rb") as f:
        magic = f.read(2)
    return magic == b"MZ"


def is_valid_macho(path):
    with open(path, "rb") as f:
        magic = f.read(4)
    return magic in (
        b"\xfe\xed\xfa\xce",
        b"\xce\xfa\xed\xfe",
        b"\xfe\xed\xfa\xcf",
        b"\xcf\xfa\xed\xfe",
        b"\xca\xfe\xba\xbe",
        b"\xbe\xba\xfe\xca",
    )


def check_ofx_symbols(path):
    ext = os.path.splitext(path)[1]
    if ext != ".so":
        return []
    try:
        out = subprocess.check_output(
            ["nm", "-D", path], stderr=subprocess.STDOUT, text=True
        )
    except (FileNotFoundError, subprocess.CalledProcessError):
        return ["nm not available; skipped OFX symbol check"]
    exported = set()
    for line in out.splitlines():
        match = re.match(r"^[0-9a-f]+\s+T\s+(\S+)", line)
        if match:
            exported.add(match.group(1))
    missing = [s for s in OFX_REQUIRED_SYMBOLS if s not in exported]
    return [f"missing OFX symbol: {s}" for s in missing]


def check_binary(path):
    ext = os.path.splitext(path)[1]
    if ext == ".so":
        return is_valid_elf(path)
    elif ext == ".dll":
        return is_valid_pe(path)
    elif ext == ".bundle":
        return os.path.isdir(path) or is_valid_macho(path)
    elif ext == ".dylib":
        return is_valid_macho(path)
    return True


def verify_tgz(path):
    errors = []
    found_plugins = set()
    found_readme = False
    tmp_dir = None

    try:
        tmp_dir = tempfile.mkdtemp(prefix="metrovfx_verify_")
        with tarfile.open(path, "r:gz") as tar:
            tar.extractall(path=tmp_dir)
            members = tar.getnames()

        prefix = os.path.commonpath(members)
        extract_root = os.path.join(tmp_dir, prefix) if prefix else tmp_dir

        for name in members:
            basename = os.path.basename(name)
            full = os.path.join(tmp_dir, name)

            if basename == EXPECTED_README:
                found_readme = True
                if os.path.getsize(full) == 0:
                    errors.append(f"{EXPECTED_README} is empty")

            for plugin in EXPECTED_PLUGINS:
                stem = os.path.splitext(basename)[0]
                if stem == plugin:
                    found_plugins.add(plugin)
                    if not check_binary(full):
                        errors.append(f"{basename} is not a valid binary")
                    if os.path.getsize(full) == 0:
                        errors.append(f"{basename} is empty (0 bytes)")
                    errors.extend(check_ofx_symbols(full))

        missing = set(EXPECTED_PLUGINS) - found_plugins
        if missing:
            errors.append(f"Missing plugins: {', '.join(sorted(missing))}")

        if not found_readme:
            errors.append(f"Missing {EXPECTED_README}")

        extra = [
            n
            for n in members
            if os.path.isfile(os.path.join(tmp_dir, n))
            and os.path.basename(n) not in [p + ".so" for p in EXPECTED_PLUGINS]
            and os.path.basename(n) != EXPECTED_README
        ]
        if extra:
            errors.append(f"Unexpected files in archive: {extra}")

    finally:
        if tmp_dir and os.path.isdir(tmp_dir):
            shutil.rmtree(tmp_dir, ignore_errors=True)

    return errors


def main():
    if len(sys.argv) < 2:
        print("Usage: verify-installer.py <archive>")
        sys.exit(1)

    path = sys.argv[1]
    if not os.path.isfile(path):
        print(f"ERROR: file not found: {path}")
        sys.exit(1)

    print(f"Verifying: {path}")
    print(f"  Size: {os.path.getsize(path):,} bytes")

    ext = os.path.splitext(path)[1]
    if path.endswith(".tar.gz") or ext == ".tgz":
        errors = verify_tgz(path)
    else:
        print(f"ERROR: unsupported archive format: {path}")
        sys.exit(1)

    if errors:
        print(f"\nFAILED ({len(errors)} issue(s)):")
        for e in errors:
            print(f"  - {e}")
        sys.exit(1)
    else:
        print("\nPASSED")
        print(f"  Plugins: {len(EXPECTED_PLUGINS)}/11 present and valid")
        print(f"  README:  included")
        sys.exit(0)


if __name__ == "__main__":
    main()
