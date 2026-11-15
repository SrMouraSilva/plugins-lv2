#!/usr/bin/env python3
# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""Pins the recipes of store/*.mk to a commit of this repository (the one they build).

    python3 tools/store/pin.py            # the commit that is checked out (HEAD)
    python3 tools/store/pin.py 1a2b3c4    # any commit; it is written in full

The commit must exist on GitHub (push first), because the builder clones the repository to build it.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    ref = sys.argv[1] if len(sys.argv) > 1 else "HEAD"
    commit = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--verify", f"{ref}^{{commit}}"],
                            capture_output=True, text=True, check=True).stdout.strip()

    for recipe in sorted((ROOT / "store").glob("*.mk")):
        text = recipe.read_text()
        pinned, count = re.subn(r"^(\w+_VERSION = ).*$", rf"\g<1>{commit}", text, count=1, flags=re.M)
        if count != 1:
            print(f"{recipe.name}: no _VERSION line", file=sys.stderr)
            return 1
        recipe.write_text(pinned)
        print(f"{recipe.name}: {commit}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
