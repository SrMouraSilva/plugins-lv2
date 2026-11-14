#!/usr/bin/env python3
# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""
Builds the documentation site (static HTML, for GitHub Pages) into a folder:

    python3 tools/site/build_site.py --out _site [--repo OWNER/NAME] [--ref BRANCH]

    _site/index.html                    the landing page: the plugins, the guides, the downloads
    _site/plugins/<plugin>/manual.html  the user manual of each plugin (built from its docs/manual.md)
    _site/plugins/<plugin>/*.webp|gif   the picture and the GIFs shown on the landing page
    _site/guides/<guide>.html           the guides for developers (docs/*.md and the preview's README)

The manuals and the guides are rendered by tools/manual/build_manual_html.py, so they look like the manuals the device
serves. A link from a guide to another guide becomes a link to its page; a link to any other file of the repository
becomes a link to that file on GitHub (at --ref). Needs `pip install markdown pillow`.
"""
import argparse
import html
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "manual"))
import build_manual_html as manual  # noqa: E402

RESOURCES = ROOT / "plugins" / "resources"

# The order of the Readme
PLUGINS = ["gossiper", "tetr4-switch", "hept7-switch"]

# (source, page, one line for the card)
GUIDES = [
    ("docs/custom-settings-pages.md", "custom-settings-pages",
     "A full-screen, touch-first settings page for your plugin: what MOD UI expects, fitting any screen with pages, accessibility and the cache."),
    ("tools/modgui-preview/README.md", "modgui-preview",
     "Look at, and click through, the interface of a plugin in your browser, on a phone or a tablet as MOD UI shows itself there. No device needed."),
    ("docs/making-gifs.md", "making-gifs",
     "Record a clean, reproducible GIF of your plugin being used, from a script, and keep it up to date with one command."),
    ("docs/alternative-thumbnails.md", "alternative-thumbnails",
     "Draw the simple, abstract picture of a plugin that stores show first, as light WebP with transparency."),
    ("docs/html-manuals.md", "html-manuals",
     "Write a manual once in Markdown and build a self-contained HTML page for the device: cover, steps, callouts, videos, dark theme."),
]

# The title of a guide card, when the first line of its file is not a good one
GUIDE_TITLES = {"modgui-preview": "Previewing a modgui in the browser"}

GUIDE_TAGLINES = {
    "custom-settings-pages": "A settings page that fits every screen.",
    "modgui-preview": "The interface of your plugin, in a browser.",
    "making-gifs": "A GIF of your plugin, from a script.",
    "alternative-thumbnails": "A picture that says what the plugin is.",
    "html-manuals": "One Markdown file, one beautiful manual.",
}

GUIDE_THEME = {
    "eyebrow": "SrMouraSilva · Developer guide",
    "edition": "Developer guide",
    "version": "",
    "hero": "",
    "brand": "#1d2358",
    "brand2": "#0b0e2b",
    "accent": "#3342c4",
    "accentDark": "#a9b6ff",
    "colors": ["#ff6667", "#ffc861", "#66ffff", "#bb7cd2"],
}


def git(*args: str) -> str:
    try:
        return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    except Exception:
        return ""


def repo_from_git() -> str:
    match = re.search(r"github\.com[:/]([^/]+/[^/.]+)", git("config", "remote.origin.url"))
    return match.group(1) if match else "SrMouraSilva/plugins-lv2"


def link_fixer(source: Path, guides: dict, repo: str, ref: str):
    """What a link of a guide points to on the site: another guide, a manual, or a file of the repository on GitHub"""
    def fix(href: str) -> str:
        if re.match(r"(https?:|mailto:|#|data:)", href):
            return href
        target, hash_, fragment = href.partition("#")
        resolved = (source.parent / target).resolve()
        try:
            relative = resolved.relative_to(ROOT)
        except ValueError:
            return href
        if relative in guides:
            return f"{guides[relative]}.html{hash_}{fragment}"
        manual_of = re.match(r"plugins/resources/([^/]+)\.lv2/docs/manual\.md$", relative.as_posix())
        if manual_of:
            return f"../plugins/{manual_of.group(1)}/manual.html{hash_}{fragment}"
        kind = "tree" if resolved.is_dir() else "blob"
        return f"https://github.com/{repo}/{kind}/{ref}/{relative.as_posix()}{hash_}{fragment}"
    return fix


def esc(text: str) -> str:
    return html.escape(text, quote=True)


def plugin_data(name: str) -> dict:
    bundle = RESOURCES / f"{name}.lv2"
    config = json.loads((bundle / "docs" / "manual.json").read_text())
    return {"name": name, "bundle": bundle, **config}


def build(out: Path, repo: str, ref: str) -> None:
    if out.exists():
        shutil.rmtree(out)
    (out / "plugins").mkdir(parents=True)
    (out / "guides").mkdir()
    (out / ".nojekyll").write_text("")

    # --- the manuals and the pictures of the landing page
    plugins = []
    for name in PLUGINS:
        data = plugin_data(name)
        target = out / "plugins" / name
        target.mkdir()
        manual.build(name, target / "manual.html")
        docs = data["bundle"] / "docs"
        for source, dest in [(docs / "alternative-thumbnail.webp", "thumbnail.webp"),
                             (docs / "images" / f"{name}-demo.gif", "demo.gif"),
                             (docs / "images" / f"{name}-settings.gif", "settings.gif")]:
            if source.exists():
                shutil.copy(source, target / dest)
        plugins.append(data)

    # --- the guides
    guides = {Path(source): page for source, page, _ in GUIDES}
    for source, page, _ in GUIDES:
        path = ROOT / source
        config = {**GUIDE_THEME, "tagline": GUIDE_TAGLINES[page], **({"title": GUIDE_TITLES[page]} if page in GUIDE_TITLES else {}),
                  "links": [{"label": "All the documentation", "url": "../index.html"},
                            {"label": "Source on GitHub", "url": f"https://github.com/{repo}/blob/{ref}/{source}"}]}
        document = manual.render(path.read_text(encoding="utf-8"), path.parent, config, page,
                                 fix_links=link_fixer(path, guides, repo, ref))
        (out / "guides" / f"{page}.html").write_text(document, encoding="utf-8")
        print(f"guides/{page}.html: {len(document) // 1024} KB")

    (out / "index.html").write_text(index(plugins, repo, ref), encoding="utf-8")
    print("index.html")


def index(plugins: list, repo: str, ref: str) -> str:
    sha, date = git("rev-parse", "--short=7", "HEAD"), git("log", "-1", "--format=%cd", "--date=short")
    css = (ROOT / "tools/manual/manual.css").read_text() + (HERE / "site.css").read_text()

    def card(p: dict) -> str:
        name = p["name"]
        accent = p["colors"][0]
        chips = "".join(f"<span>{esc(b)}</span>" for b in p.get("badges", []))
        actions = f'<a class="btn primary" href="plugins/{name}/manual.html">User manual</a>' + "".join(
            f'<a class="btn" href="{esc(l["url"])}" target="_blank" rel="noopener">{esc(l["label"])}</a>' for l in p.get("links", []))
        strip = ", ".join(f"{c} {i * 100 // len(p['colors'])}% {(i + 1) * 100 // len(p['colors'])}%" for i, c in enumerate(p["colors"]))
        demo = ""
        if (ROOT / "plugins/resources" / f"{name}.lv2/docs/images/{name}-demo.gif").exists():
            demo = (f'<details class="demo"><summary>See it in action</summary>'
                    f'<img src="plugins/{name}/demo.gif" alt="{esc(p["title"])} on a pedalboard" loading="lazy">'
                    f'<img src="plugins/{name}/settings.gif" alt="The settings page of {esc(p["title"])}" loading="lazy"></details>')
        return (f'<article class="plugin" style="--a:{accent}; --strip: linear-gradient(90deg, {strip})">'
                f'<div class="plugin-head"><img class="plugin-thumb" src="plugins/{name}/thumbnail.webp" alt="" width="104" height="104">'
                f'<div><h3>{esc(p["title"])}</h3><p class="tag">{esc(p.get("tagline", ""))}</p></div></div>'
                f'<p>{esc(p.get("subtitle", ""))}</p><p class="chips">{chips}</p>'
                f'<p class="actions">{actions}</p>{demo}</article>')

    guide_cards = "".join(
        f'<a class="guide" href="guides/{page}.html"><b>{esc(GUIDE_TITLES.get(page) or re.sub(r"^# ", "", next(line for line in (ROOT / source).read_text().splitlines() if line.startswith("# "))))}</b>'
        f'<span>{esc(text)}</span><i aria-hidden="true">→</i></a>'
        for source, page, text in GUIDES)

    trio = "".join(f'<img src="plugins/{p["name"]}/thumbnail.webp" alt="{esc(p["title"])}">' for p in plugins)
    badges = "".join(f"<span>{esc(p['title'])}</span>" for p in plugins)
    colors = ["#ff6667", "#ffc861", "#2ee6d6", "#5b97ff", "#bb7cd2"]     # one per plugin, and the colours they share
    strip = ", ".join(f"{c} {i * 100 // len(colors)}% {(i + 1) * 100 // len(colors)}%" for i, c in enumerate(colors))

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>SrMouraSilva plugins — documentation</title>
<meta name="description" content="Manuals, guides and downloads of the LV2 utility plugins Gossiper, Tetr4 Switch and Hept7 Switch for MOD Dwarf, MOD Duo X and MOD Duo.">
<style>
:root {{ --brand: #1d2358; --brand-2: #0b0e2b; --accent: #3342c4; --accent-dark: #a9b6ff; --strip: linear-gradient(90deg, {strip}); }}
{css}
</style>
</head>
<body>
<header class="cover">
  <div class="cover-inner">
    <div class="cover-text">
      <p class="cover-eyebrow">SrMouraSilva · Documentation</p>
      <h1>Plugins for MOD devices</h1>
      <p class="cover-tagline">Manuals, guides and downloads.</p>
      <p class="cover-sub">LV2 utility plugins for MOD Dwarf, MOD Duo X and MOD Duo, and everything we learned making their touch-friendly interfaces.</p>
      <p class="cover-badges">{badges}</p>
      <p class="actions hero-actions"><a class="btn primary on-dark" href="https://github.com/{repo}/releases">Download</a><a class="btn on-dark" href="https://github.com/{repo}" target="_blank" rel="noopener">Source code</a></p>
    </div>
    <div class="trio" aria-hidden="true">{trio}</div>
  </div>
  <div class="cover-strip"></div>
</header>
<main class="site">
  <section class="block" id="plugins">
    <h2>The plugins</h2>
    <p class="lead">Each one has a user manual, written for someone who has never used it.</p>
    <div class="plugins">{"".join(card(p) for p in plugins)}</div>
  </section>
  <section class="block" id="guides">
    <h2>Build your own</h2>
    <p class="lead">The guides for plugin developers: what we learned making these, written so that you can do the same.</p>
    <div class="guides">{guide_cards}</div>
  </section>
  <section class="block" id="download">
    <h2>Download</h2>
    <p class="lead">Builds for <b>MOD Duo</b>, <b>MOD Duo X</b> and <b>MOD Dwarf</b> are published on the
    <a href="https://github.com/{repo}/releases">Releases</a> page for every commit; the one marked <b>Latest</b> is the most recent build of the main branch.
    Each archive holds the <code>.lv2</code> bundle folder.</p>
  </section>
</main>
<footer class="site-footer">
  <div class="footer-strip"></div>
  <p><a href="https://github.com/{repo}">{esc(repo)}</a> · Apache-2.0 · built from {esc(ref)} @ {esc(sha)} {esc(date)}</p>
</footer>
</body>
</html>
"""


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=ROOT / "_site")
    parser.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY") or repo_from_git())
    parser.add_argument("--ref", default=os.environ.get("GITHUB_REF_NAME") or git("branch", "--show-current") or "master")
    args = parser.parse_args()
    build(args.out.resolve(), args.repo, args.ref)
