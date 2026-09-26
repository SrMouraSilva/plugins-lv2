#!/usr/bin/env python3
"""
Builds modgui/manual.pdf from docs/manual.md (+ docs/images/*).

manual.md is the single source of truth; this script renders it to HTML
and prints that HTML to PDF with headless Chrome, so re-running it after
editing manual.md keeps the shipped PDF in sync.

Usage: python3 build_manual.py
Requires: `pip install markdown` and Google Chrome (or set CHROME_BIN).
"""
import os
import shutil
import subprocess
import sys

import markdown

DOCS_DIR = os.path.dirname(os.path.abspath(__file__))
BUNDLE_DIR = os.path.dirname(DOCS_DIR)
MANUAL_MD = os.path.join(DOCS_DIR, "manual.md")
OUTPUT_PDF = os.path.join(BUNDLE_DIR, "modgui", "manual.pdf")

CHROME_CANDIDATES = [
    os.environ.get("CHROME_BIN"),
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
    "google-chrome",
    "chromium",
    "chromium-browser",
]

CSS = """
@page { size: A4; margin: 20mm 18mm; }
body {
  font-family: -apple-system, Helvetica, Arial, sans-serif;
  color: #1a1a1a;
  line-height: 1.5;
  font-size: 12pt;
}
h1 { font-size: 22pt; color: #02094E; margin-bottom: 4pt; }
h2 { font-size: 15pt; color: #02094E; margin-top: 22pt; border-bottom: 1px solid #ddd; padding-bottom: 4pt; }
h3 { font-size: 12.5pt; color: #02094E; }
img { max-width: 100%; display: block; margin: 10pt auto; page-break-inside: avoid; }
table { border-collapse: collapse; width: 100%; margin: 8pt 0; font-size: 10.5pt; }
th, td { border: 1px solid #ccc; padding: 6pt 8pt; text-align: left; vertical-align: top; }
th { background: #F4F4FA; color: #02094E; }
code { background: #F4F4FA; padding: 1pt 4pt; border-radius: 3pt; font-size: 10pt; }
blockquote { border-left: 3px solid #FF6768; margin: 8pt 0; padding: 2pt 12pt; color: #333; background: #FAFAFC; }
a { color: #02094E; }
"""


def render_html(md_path: str) -> str:
    with open(md_path, "r", encoding="utf-8") as f:
        text = f.read()

    title = next(
        (line.lstrip("#").strip() for line in text.splitlines() if line.startswith("# ")),
        "User Manual",
    )
    body = markdown.markdown(text, extensions=["tables", "fenced_code", "md_in_html"])
    return f"""<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>{title}</title>
<style>{CSS}</style>
</head>
<body>
{body}
</body>
</html>"""


def find_chrome() -> str:
    for candidate in CHROME_CANDIDATES:
        if not candidate:
            continue
        if os.path.isabs(candidate) and os.path.exists(candidate):
            return candidate
        found = shutil.which(candidate)
        if found:
            return found
    raise RuntimeError("Could not find Google Chrome/Chromium. Set CHROME_BIN.")


def main():
    html = render_html(MANUAL_MD)
    html_path = os.path.join(DOCS_DIR, "_manual_build.html")
    with open(html_path, "w", encoding="utf-8") as f:
        f.write(html)

    os.makedirs(os.path.dirname(OUTPUT_PDF), exist_ok=True)

    chrome = find_chrome()
    subprocess.run(
        [
            chrome,
            "--headless",
            "--disable-gpu",
            "--no-sandbox",
            f"--print-to-pdf={OUTPUT_PDF}",
            "--no-pdf-header-footer",
            "--print-to-pdf-no-header",
            f"file://{html_path}",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    os.remove(html_path)
    print(f"Wrote {OUTPUT_PDF}")


if __name__ == "__main__":
    sys.exit(main())
