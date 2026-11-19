#!/usr/bin/env python3
# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""Packs the plugins built for the computer, to be used in MOD Desktop (macOS, Windows, Linux).

    cd plugins && make build BINARY_EXT=.dylib MACOS=true CFLAGS="..." LDFLAGS="..."   # the build of the system, see below
    python3 tools/desktop/package.py macos --out desktop-dist

A bundle of a MOD unit holds an ARM Linux binary (`.so`) that MOD Desktop cannot load: it needs a native one, `.dylib` on macOS
(universal: arm64 and x86_64), `.dll` on Windows (64 bits) and `.so` on Linux (x86_64). This script takes what `make build` left in
plugins/resources/<plugin>.lv2 and writes, for each plugin, `<out>/<plugin>-<os>.lv2.zip` with one folder, `<plugin>.lv2`, that goes
in `Documents/MOD Desktop/lv2/` (https://github.com/mod-audio/mod-desktop/blob/main/docs/plugins.md). The folder is the bundle of the
repository without `docs/` and without the binaries of other systems, with the binary of this one and its manifest.ttl saying so
(`lv2:binary <plugin.dylib>`). On macOS the binary is signed ad hoc (`codesign -s -`): Apple Silicon refuses an unsigned one, and the
signature the linker adds is lost when the file is changed after the link.

Builds, which .github/workflows/release.yml runs on a runner of each system (the flags are there):
    macos    brew install lv2; MACOS=true BINARY_EXT=.dylib, `-arch arm64 -arch x86_64 -mmacosx-version-min=10.15` in CFLAGS and LDFLAGS
    windows  MSYS2 MINGW64 (gcc, make, lv2); WIN32=true BINARY_EXT=.dll
    linux    apt install lv2-dev; the default (.so), on the oldest runner there is, for the oldest glibc
"""
import argparse
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RESOURCES = ROOT / "plugins" / "resources"

EXTENSIONS = {"macos": ".dylib", "windows": ".dll", "linux": ".so"}
BINARIES = tuple(EXTENSIONS.values())


def plugins() -> list:
    return sorted(path.name[: -len(".lv2")] for path in RESOURCES.glob("*.lv2"))


def run(*command: str) -> str:
    return subprocess.run(command, capture_output=True, text=True, check=True).stdout.strip()


def stage(name: str, system: str, target: Path) -> Path:
    """The bundle of `name` for `system`, in <target>/<name>.lv2"""
    extension = EXTENSIONS[system]
    source = RESOURCES / f"{name}.lv2"
    binary = source / f"{name}{extension}"
    if not binary.is_file():
        raise SystemExit(f"{binary} is not there: build the plugins first (make build BINARY_EXT={extension} ...)")

    bundle = target / f"{name}.lv2"
    # docs/ is for people (it is not installed on a unit either); the other binaries are the ones of other systems
    shutil.copytree(source, bundle, ignore=shutil.ignore_patterns("docs", "__pycache__", ".DS_Store", *(f"*{e}" for e in BINARIES)))
    shutil.copy2(binary, bundle / binary.name)

    manifest = bundle / "manifest.ttl"
    text = manifest.read_text(encoding="utf-8")
    text, count = re.subn(r"(lv2:binary\s*<)[^>]*(>)", rf"\g<1>{name}{extension}\g<2>", text)
    if count != 1:
        raise SystemExit(f"{manifest}: expected one lv2:binary, found {count}")
    manifest.write_text(text, encoding="utf-8")

    if system == "macos":
        run("codesign", "--force", "--sign", "-", str(bundle / binary.name))
        archs = run("lipo", "-archs", str(bundle / binary.name)).split()
        if not {"arm64", "x86_64"} <= set(archs):
            raise SystemExit(f"{binary.name} is {' '.join(archs)}: MOD Desktop on macOS needs arm64 and x86_64 (a universal build)")
    return bundle


def pack(bundle: Path, archive: Path) -> None:
    files = sorted(path for path in bundle.rglob("*") if path.is_file())
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zipped:
        for path in files:
            zipped.write(path, Path(bundle.name) / path.relative_to(bundle))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("system", choices=sorted(EXTENSIONS))
    parser.add_argument("--out", type=Path, default=ROOT / "desktop-dist")
    parser.add_argument("plugins", nargs="*", help="what to pack (default: every bundle of plugins/resources)")
    args = parser.parse_args()

    names = args.plugins or plugins()
    args.out.mkdir(parents=True, exist_ok=True)
    staging = args.out / f".stage-{args.system}"
    shutil.rmtree(staging, ignore_errors=True)
    staging.mkdir()
    for name in names:
        bundle = stage(name, args.system, staging)
        archive = args.out / f"{name}-{args.system}.lv2.zip"
        pack(bundle, archive)
        print(f"{archive.name}: {archive.stat().st_size // 1024} KB")
    shutil.rmtree(staging)
    return 0


if __name__ == "__main__":
    sys.exit(main())
