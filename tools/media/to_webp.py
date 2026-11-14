# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""
Converts PNG pictures to WebP (several times lighter, and the transparency is kept):

    python3 tools/media/to_webp.py FILE.png ...              # lossy, quality 90: screenshots and photos
    python3 tools/media/to_webp.py --lossless FILE.png ...   # exact pixels: drawings with flat colours and sharp text
    python3 tools/media/to_webp.py --docs                    # every PNG of plugins/resources/*.lv2/docs

The PNG is removed once the WebP is written (use --keep to leave it), and the Markdown that mentions it is rewritten
(`name.png` -> `name.webp`) in the docs/ of the same bundle. Needs Pillow. The pictures the tools of this folder take
are already WebP: this is for pictures that come from somewhere else.
"""
import argparse
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent.parent


def convert(path: Path, lossless: bool, keep: bool) -> None:
    out = path.with_suffix(".webp")
    image = Image.open(path)
    image.save(out, "WEBP", lossless=lossless, quality=100 if lossless else 90, method=6)
    print(f"{path.relative_to(ROOT)}: {path.stat().st_size // 1024} KB -> {out.stat().st_size // 1024} KB")

    # Markdown that points to the old name, next to the bundle's docs
    docs = path.parent if path.parent.name == "docs" else path.parent.parent
    for md in docs.glob("*.md"):
        text = md.read_text(encoding="utf-8")
        for old in {f"images/{path.name}", path.name} if path.parent.name == "images" else {path.name}:
            text = text.replace(old, old[: -len(".png")] + ".webp")
        md.write_text(text, encoding="utf-8")
    if not keep:
        path.unlink()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("files", nargs="*", type=Path)
    parser.add_argument("--lossless", action="store_true")
    parser.add_argument("--keep", action="store_true")
    parser.add_argument("--docs", action="store_true")
    args = parser.parse_args()

    files = [f.resolve() for f in args.files]
    if args.docs:
        files += sorted((ROOT / "plugins" / "resources").glob("*.lv2/docs/**/*.png"))
    for path in files:
        convert(path, args.lossless, args.keep)


if __name__ == "__main__":
    main()
