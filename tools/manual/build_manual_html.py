#!/usr/bin/env python3
# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""
Builds a single, self-contained HTML manual (modgui/manual.html, or the `output` of docs/manual.json) from docs/manual.md of a plugin bundle.

    python3 tools/manual/build_manual_html.py tetr4-switch          # one plugin
    python3 tools/manual/build_manual_html.py all                   # every bundle that has docs/manual.md

MOD UI opens modgui:documentation in a new tab, from /effect/file/documentation?uri=..., so relative links do not
work there: the page carries its CSS, its JavaScript and every image (as data: URIs). Needs `pip install markdown`
and, to shrink big screenshots, Pillow. See docs/html-manuals.md for the Markdown conventions and docs/manual.json.
"""
import base64
import html
import io
import json
import mimetypes
import re
import sys
from pathlib import Path

import markdown

HERE = Path(__file__).resolve().parent
RESOURCES = HERE.parent.parent / "plugins" / "resources"

DEFAULTS = {
    "eyebrow": "Plugin manual",
    "edition": "User manual",
    "hero": "../modgui/screenshot.png",
    "brand": "#1b2a6b",
    "brand2": "#0b1238",
    "accent": "#2d46c9",
    "accentDark": "#9fb0ff",
    "colors": ["#ff6667", "#ffff67", "#66ffff", "#bb7cd2"],
    "links": [],
    # Where the page is written, relative to the bundle: modgui/ is installed on the device, docs/ is not
    "output": "modgui/manual.html",
}

BIG_IMAGE_BYTES = 150 * 1024


# --------------------------------------------------------------------------------------------------------------------
# Markdown extensions of this builder

def recipe_block(block: str, colors: list) -> str:
    """
    ```recipe
    Title
    cols: 1 2 3 4
    Row name: 0101      (1 = on)
    ---
    A paragraph.
    assign: Out 1 -> something
    ```
    """
    lines = [line.rstrip() for line in block.strip().splitlines()]
    title, cols, rows, body, assign, in_body = lines[0], [], [], [], "", False
    for line in lines[1:]:
        if line.strip() == "---":
            in_body = True
        elif in_body:
            if line.startswith("assign:"):
                assign = line[len("assign:"):].strip()
            elif line.strip():
                body.append(line.strip())
        elif line.startswith("cols:"):
            cols = line[len("cols:"):].split()
        elif ":" in line:
            name, bits = line.rsplit(":", 1)
            rows.append((name.strip(), bits.strip()))

    header = "".join(f"<span>{html.escape(c)}</span>" for c in cols)
    grid = "".join(
        f'<div class="recipe-row" style="--c:{colors[i % len(colors)]}"><b>{html.escape(name)}</b>'
        + "".join(f'<i class="{"on" if bit == "1" else ""}"></i>' for bit in bits) + "</div>"
        for i, (name, bits) in enumerate(rows)
    )
    text = "".join(f"<p>{markdown.markdown(p, extensions=[])[3:-4]}</p>" for p in body)
    assign_html = f'<p class="assign">{markdown.markdown(assign)[3:-4]}</p>' if assign else ""
    return (f'\n<div class="recipe"><div class="recipe-grid"><div class="recipe-cols"><b></b>{header}</div>{grid}</div>'
            f'<div class="recipe-body"><h4>{html.escape(title)}</h4>{text}{assign_html}</div></div>\n')


def preprocess(source: str, colors: list) -> str:
    """
    One pass over the lines, tracking fenced code blocks:
      * a ```recipe block (at the left margin) becomes a recipe card;
      * any other fenced block at the left margin is left exactly as it is, whatever it contains (an example of a recipe
        inside a ````markdown block stays an example);
      * a fenced block indented inside a list item becomes a code block of its own after the item: Python-Markdown only
        understands fences at the left margin, and without this the backticks pair up with others and spoil the page.
    """
    lines, out, i = source.split("\n"), [], 0
    while i < len(lines):
        margin = re.match(r"^(`{3,}|~{3,})(\S*)\s*$", lines[i])
        nested = re.match(r"^( +)(`{3,}|~{3,})(\S*)\s*$", lines[i])
        if margin or nested:
            indent = len(nested.group(1)) if nested else 0
            fence, info = (margin or nested).group(margin and 1 or 2), (margin or nested).group(margin and 2 or 3)
            j = i + 1
            while j < len(lines) and not re.match(rf"^ {{0,{indent + 3}}}{re.escape(fence)}\s*$", lines[j]):
                j += 1
            code = [line[indent:] if line.startswith(" " * indent) else line.lstrip() for line in lines[i + 1:j]]
            if margin and info == "recipe":
                out.append(recipe_block("\n".join(code), colors))
            elif nested:
                out += ["", f'<pre><code>{html.escape(chr(10).join(code), quote=False)}</code></pre>', ""]
            else:
                out += lines[i:j + 1]
            i = j + 1
        else:
            out.append(lines[i])
            i += 1
    return "\n".join(out)


ALERT = re.compile(r"<blockquote>\s*<p>\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]([^\n<]*)(?:<br\s*/?>\s*)?\n?", re.I)
ALERT_TITLES = {"note": "Good to know", "tip": "Tip", "important": "Important", "warning": "Heads-up", "caution": "Heads-up"}
ALERT_KINDS = {"note": "note", "tip": "tip", "important": "important", "warning": "warn", "caution": "warn"}


def alerts(page: str) -> str:
    """> [!NOTE] Title  (GitHub's alert syntax, with an optional title) and plain blockquotes become callouts"""
    def one(match: re.Match) -> str:
        kind = match.group(1).lower()
        title = match.group(2).strip() or ALERT_TITLES[kind]
        return f'<blockquote class="callout {ALERT_KINDS[kind]}"><p class="callout-title">{html.escape(title)}</p><p>'
    page = ALERT.sub(one, page)
    return page.replace("<blockquote>", '<blockquote class="callout plain">')


VIDEO = re.compile(r'<p><a href="(https?://[^"]+)"[^>]*><img alt="([^"]*)" src="([^"]*)"[^>]*/?></a></p>')


def videos(page: str) -> str:
    """
    [![Title — description](thumbnail)](https://youtu.be/ID)  becomes a card with a play button. The same with a link
    to anywhere else and a " — " in the text is a card without the button (an example pedalboard, a store page)
    """
    def one(match: re.Match) -> str:
        url, alt, src = match.groups()
        video = bool(re.match(r"https?://(youtu\.be|(www\.)?youtube\.com)", url))
        if not video and " — " not in alt:
            return match.group(0)
        title, _, text = alt.partition(" — ")
        small = f"<small>{text}</small>" if text else ""
        play = '<i class="play" aria-hidden="true"></i>' if video else ""
        return (f'<a class="media{"" if video else " link"}" href="{url}" target="_blank" rel="noopener"><span class="thumb"><img src="{src}" alt="" loading="lazy">'
                f'{play}</span><span class="meta"><b>{title}</b>{small}</span></a>')
    return VIDEO.sub(one, page)


FIGURE = re.compile(r'<p><img alt="([^"]*)" src="([^"]*)"\s*/?></p>')


def figures(page: str, base: Path) -> str:
    """An image alone in a paragraph is a figure; its alt text is the caption. A small picture (a phone, a GIF) is not stretched"""
    def one(match: re.Match) -> str:
        alt, src = match.groups()
        caption = f"<figcaption>{alt}</figcaption>" if alt and alt.lower() != "alt text" else ""
        style = ""
        try:
            from PIL import Image
            width = Image.open(base / src).width
            if width < 760:
                style = f' style="max-width:{width}px"'
        except Exception:
            pass
        return f'<figure{style}><img src="{src}" alt="{alt}" loading="lazy">{caption}</figure>'
    return FIGURE.sub(one, page)


def faq(section: str) -> str:
    """In a section called Questions / FAQ every h3 and what follows it becomes a collapsible question"""
    parts = re.split(r"(?=<h3[ >])", section)
    out = [parts[0]]
    for part in parts[1:]:
        head, _, rest = part.partition("</h3>")
        question = re.sub(r"<h3[^>]*>", "", head)
        out.append(f"<details><summary>{question}</summary><div class=\"answer\">{rest}</div></details>")
    return "".join(out)


def tables(page: str) -> str:
    return re.sub(r"(<table>.*?</table>)", r'<div class="table-wrap">\1</div>', page, flags=re.S)


# --------------------------------------------------------------------------------------------------------------------
# Images are embedded: the page must work on its own

def data_uri(path: Path) -> str:
    raw = path.read_bytes()
    mime = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
    if len(raw) > BIG_IMAGE_BYTES and mime in ("image/png", "image/jpeg"):
        try:
            from PIL import Image
            image = Image.open(io.BytesIO(raw))
            buffer = io.BytesIO()
            image.save(buffer, "WEBP", quality=86, method=6)
            if buffer.tell() < len(raw):
                raw, mime = buffer.getvalue(), "image/webp"
        except Exception:
            pass
    return f"data:{mime};base64,{base64.b64encode(raw).decode('ascii')}"


def embed(page: str, base: Path) -> str:
    """Every local image becomes a data: URI. Code blocks are left alone: they show examples, not pictures"""
    def one(match: re.Match) -> str:
        src = match.group(2)
        if re.match(r"(data:|https?:|#)", src):
            return match.group(0)
        path = (base / src).resolve()
        if not path.exists():
            print(f"  warning: image not found: {src}", file=sys.stderr)
            return match.group(0)
        return f'{match.group(1)}"{data_uri(path)}"'

    parts = re.split(r"(<pre>.*?</pre>)", page, flags=re.S)
    return "".join(part if part.startswith("<pre>") else re.sub(r'(\bsrc=)"([^"]+)"', one, part) for part in parts)


# --------------------------------------------------------------------------------------------------------------------

def github_slug(text: str) -> str:
    """The anchor GitHub gives a heading (lowercase, punctuation removed, spaces to dashes): what a table of contents links to"""
    text = re.sub(r"<[^>]+>", "", text).lower()
    return re.sub(r"\s", "-", re.sub(r"[^\w\s-]", "", text, flags=re.U))


def slug(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", re.sub(r"<[^>]+>", "", text).lower()).strip("-")


def render(source: str, docs: Path, config: dict, fallback_title: str = "Manual", fix_links=None) -> str:
    """
    The page, as text, for a Markdown source. docs: where its images are looked up. config: the fields of manual.json
    (the defaults are filled in). fix_links: an optional function that rewrites the href of every link (the site uses
    it to point a link to another page of the site, or to a file of the repository)
    """
    config = {**DEFAULTS, **config}

    # The title: the first "# ", which becomes the cover
    h1 = re.search(r"^# (.+)$", source, flags=re.M)
    title = config.get("title") or (re.sub(r"\s+[—-]\s+(User Manual|Guide)\s*$", "", h1.group(1)) if h1 else fallback_title)
    if h1:
        source = source.replace(h1.group(0), "", 1)

    source = preprocess(source, config["colors"])
    body = markdown.markdown(source, extensions=["tables", "fenced_code", "attr_list", "def_list", "sane_lists", "md_in_html", "toc"],
                             extension_configs={"toc": {"slugify": lambda value, sep: github_slug(value)}})
    body = alerts(videos(figures(body, docs)))

    # Sections: what comes before the first h2 is the introduction
    chunks = re.split(r"(?=<h2[ >])", body)
    intro, sections = chunks[0], chunks[1:]
    nav, cards = [], []
    for chunk in sections:
        match = re.match(r'<h2 id="([^"]+)">(.*?)</h2>', chunk, flags=re.S)
        sid, heading = match.group(1), match.group(2)
        number = re.match(r"(\d+)\.\s+(.*)", heading)
        eyebrow, heading = (f"{int(number.group(1)):02d}", number.group(2)) if number else ("", heading)
        # "## 5. Hands-free control" is #hands-free-control; GitHub's own anchor (#5-hands-free-control) keeps working too
        alias, sid = sid, slug(heading)
        content = chunk[match.end():]
        if re.search(r"question|faq", heading, re.I):
            content = faq(content)
        nav.append((sid, html.unescape(re.sub(r"<[^>]+>", "", heading))))
        cards.append(
            f'<section class="sheet" id="{sid}"><header class="sheet-head">{f"<span id=\"{alias}\"></span>" if alias != sid else ""}'
            f'{f"<span class=\"eyebrow\">{eyebrow}</span>" if eyebrow else ""}<h2>{heading}</h2></header>{tables(content)}</section>')

    intro_card = f'<section class="sheet intro" id="overview">{tables(intro)}</section>' if intro.strip() else ""
    if intro_card:
        nav.insert(0, ("overview", "Overview"))

    css = (HERE / "manual.css").read_text()
    js = (HERE / "manual.js").read_text()
    hero = (docs / config["hero"]).resolve() if config.get("hero") else None
    hero_html = f'<img class="hero-art" src="{data_uri(hero)}" alt="{html.escape(title)}">' if hero and hero.is_file() else ""
    colors = config["colors"]
    strip = ", ".join(f"{c} {i * 100 // len(colors)}% {(i + 1) * 100 // len(colors)}%" for i, c in enumerate(colors))
    links = "".join(f'<a href="{l["url"]}" target="_blank" rel="noopener">{html.escape(l["label"])}</a>' for l in config["links"])
    badges = "".join(f"<span>{html.escape(b)}</span>" for b in config.get("badges", []))

    theme = (f":root {{ --brand: {config['brand']}; --brand-2: {config['brand2']}; --accent: {config['accent']};"
             f" --accent-dark: {config['accentDark']}; --strip: linear-gradient(90deg, {strip}); }}")
    nav_html = "".join(f'<a href="#{sid}">{html.escape(text)}</a>' for sid, text in nav)

    document = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>{html.escape(title)} — User Manual</title>
<meta name="description" content="{html.escape(config.get('tagline', ''))}">
<style>
{theme}
{css}
</style>
</head>
<body>
<!-- Generated by tools/manual/build_manual_html.py from docs/manual.md: edit that file, not this one -->
<header class="cover">
  <div class="cover-inner">
    <div class="cover-text">
      <p class="cover-eyebrow">{html.escape(config['eyebrow'])}</p>
      <h1>{html.escape(title)}</h1>
      {f'<p class="cover-tagline">{html.escape(config["tagline"])}</p>' if config.get('tagline') else ''}
      {f'<p class="cover-sub">{html.escape(config["subtitle"])}</p>' if config.get('subtitle') else ''}
      <p class="cover-badges">{badges}</p>
    </div>
    {hero_html}
  </div>
  <div class="cover-strip"></div>
  <div class="cover-foot"><span>{html.escape(config['edition'])}</span><span>{html.escape(config.get('version', ''))}</span></div>
</header>
<div class="layout">
  <nav class="rail" aria-label="Sections"><p class="rail-title">{html.escape(title)}</p>{nav_html}{f'<div class="rail-links">{links}</div>' if links else ''}</nav>
  <main class="pages">
{intro_card}
{chr(10).join(cards)}
    <footer class="footer"><div class="footer-strip"></div><p>{html.escape(title)} · {html.escape(config['edition'])} {html.escape(config.get('version', ''))}</p></footer>
  </main>
</div>
<div class="lightbox" hidden><img alt=""></div>
<script>
{js}
</script>
</body>
</html>
"""
    if fix_links:
        document = re.sub(r'(<a [^>]*?href=")([^"]+)(")', lambda m: m.group(1) + fix_links(m.group(2)) + m.group(3), document)
    return embed(document, docs)


def build(plugin: str, output: Path = None) -> Path:
    """docs/manual.md (+ manual.json) of a bundle -> its manual page (config["output"], or `output`)"""
    bundle = RESOURCES / f"{plugin}.lv2"
    docs = bundle / "docs"
    config = json.loads((docs / "manual.json").read_text()) if (docs / "manual.json").exists() else {}
    document = render((docs / "manual.md").read_text(encoding="utf-8"), docs, config, plugin)
    out = output or bundle / config.get("output", DEFAULTS["output"])
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(document, encoding="utf-8")
    print(f"{out.relative_to(HERE.parent.parent) if out.is_relative_to(HERE.parent.parent) else out}: {len(document) // 1024} KB")
    return out


if __name__ == "__main__":
    targets = sys.argv[1:] or ["all"]
    if targets == ["all"]:
        targets = sorted(p.parent.parent.name[:-4] for p in RESOURCES.glob("*.lv2/docs/manual.md"))
    for target in targets:
        build(target)
