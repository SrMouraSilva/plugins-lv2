# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""
The interactive part of a manual (see docs/interactive-manuals.md): the plugin itself, loaded in a frame of the page, that the
reader can tap to learn what each part does, and guided tours of the common tasks.

A ```interactive block in docs/manual.md is replaced (by build_manual_html.py) by what `component()` returns. The frame holds the REAL files
of the bundle (the icon and settings templates, the stylesheet and the javascript of modgui/), not a copy or a drawing, so it can never fall
behind the interface. What the reader sees and what they read comes from docs/interactive.json (areas and tours).

Needs `pip install markdown`. The TTL reader is the one of the modgui preview (tools/modgui-preview/ttl.py).
"""
import base64
import html
import json
import re
import sys
from pathlib import Path

import markdown

HERE = Path(__file__).resolve().parent
ASSETS = HERE / "interactive"
sys.path.insert(0, str(HERE.parent / "modgui-preview"))
import ttl  # noqa: E402

# What the page says around the plugin (a plugin can change any of it in the "text" of its interactive.json)
TEXT = {
    "mode": "What do you want to do?",
    "explore": "Explore",
    "tours": "Guided tours",
    "view": "View",
    "showAreas": "Show areas",
    "reset": "Reset",
    "loading": "Loading the plugin…",
    "failed": "The plugin could not be loaded here. Open this manual in a browser.",
    "intro": "<strong>Tap any part of the plugin</strong> to read what it is for. It is the real interface, so you can also use it: nothing here is saved or sent anywhere. "
             "<em>Reset</em> puts it back the way it starts.",
    "phoneHint": "On a small screen the plugin is drawn small. The settings page is made for touch: try that view.",
    "parts": "Parts you can tap",
    "chooseTour": "Pick a task",
    "allTours": "All tours",
    "back": "Back",
    "next": "Next",
    "play": "Play all",
    "playHint": "<strong>Play all</strong> runs the steps by itself, and <strong>Pause</strong> stops it where it is. Or go at your own pace with <strong>Next</strong> and <strong>Back</strong>.",
    "pause": "Pause",
    "step": "Step",
    "steps": "steps",
    "of": "of",
    "watching": "Watching the tour. Press Pause to take your time.",
    "finished": "That is the end of the tour.",
    "replay": "Play again",
    "tryIt": "Try it yourself",
    "print": "This part of the manual is interactive: open the manual on a screen to try the plugin.",
}


# --------------------------------------------------------------------------------------------------------------------
# Mustache (the part of it that MOD UI's templates use): {{name}}, {{{raw}}}, {{#section}}...{{/section}}, {{^inverted}}...{{/inverted}}

TAG = re.compile(r"\{\{\{\s*([\w.]+)\s*\}\}\}|\{\{\s*([#^/!&]?)\s*([\w.]*)\s*\}\}")


def lookup(stack, path):
    parts = path.split(".")
    for context in reversed(stack):
        if isinstance(context, dict) and parts[0] in context:
            value = context[parts[0]]
            for part in parts[1:]:
                value = value.get(part) if isinstance(value, dict) else None
            return value
    return None


def mustache(template: str, context: dict, stack=None) -> str:
    stack = (stack or []) + [context]
    out, position = [], 0
    while True:
        match = TAG.search(template, position)
        if not match:
            out.append(template[position:])
            return "".join(out)
        out.append(template[position:match.start()])
        raw, kind, name = match.group(1), match.group(2), match.group(3)
        position = match.end()
        if raw:
            out.append(str(lookup(stack, raw) or ""))
        elif kind in ("#", "^"):
            # find the closing tag of this name (they nest only with other names in these templates)
            depth, end = 1, position
            for tag in TAG.finditer(template, position):
                if tag.group(3) == name and tag.group(2) in ("#", "^"):
                    depth += 1
                elif tag.group(3) == name and tag.group(2) == "/":
                    depth -= 1
                    if depth == 0:
                        end = tag
                        break
            inner = template[position:end.start()]
            position = end.end()
            value = lookup(stack, name)
            if kind == "^":
                out.append(mustache(inner, {}, stack) if not value else "")
            elif isinstance(value, list):
                out.extend(mustache(inner, item if isinstance(item, dict) else {".": item}, stack) for item in value)
            elif value:
                out.append(mustache(inner, value if isinstance(value, dict) else {}, stack))
        elif kind == "!":
            pass
        elif kind == "/":
            pass
        else:
            out.append(html.escape(str(lookup(stack, name) if lookup(stack, name) is not None else ""), quote=True))


# --------------------------------------------------------------------------------------------------------------------

def inline(text: str) -> str:
    """Markdown for a title: one line, no paragraph around it"""
    out = markdown.markdown(text)
    return out[3:-4] if out.startswith("<p>") and out.endswith("</p>") else out


def script_safe(text: str) -> str:
    """Text that sits inside a <script> element: no `</script`, no `<!--`"""
    return text.replace("</script", "<\\/script").replace("<!--", "<\\!--")


def json_safe(value) -> str:
    """JSON for a <script type="application/json">: every `<` is escaped, so nothing in it can close the element"""
    return json.dumps(value, ensure_ascii=False, separators=(",", ":")).replace("<", "\\u003c").replace("\u2028", "\\u2028").replace("\u2029", "\\u2029")


def host_css() -> str:
    """host.css, with `url(@name)` replaced by the picture vendor/mod-ui/name.png (MOD UI's own images) as a data: URI"""
    def inline_picture(match):
        raw = (ASSETS / "vendor" / "mod-ui" / f"{match.group(1)}.png").read_bytes()
        return f'url("data:image/png;base64,{base64.b64encode(raw).decode("ascii")}")'
    return re.sub(r"url\(@([\w-]+)\)", inline_picture, (ASSETS / "host.css").read_text(encoding="utf-8"))


def frame_document(bundle: Path, platform: str = "dwarf") -> tuple:
    """The page of the frame: MOD UI's part (host.css, host.js, jQuery) and the files of the plugin. Returns (document, model)"""
    model = ttl.load_bundle(bundle)
    gui = model["gui"]
    modgui = bundle / "modgui"
    read = lambda name: (modgui / name).read_text(encoding="utf-8")

    effect = {"name": model["title"], "label": model["label"], "brand": model["brand"], "color": model["color"],
              "ports": model["ports"], "parameters": model["parameters"]}
    data = {"effect": effect, "brand": model["brand"], "label": model["label"], "color": model["color"], "ns": "", "cns": "_x"}

    css = mustache(read(gui["stylesheet"]), data)
    icon = mustache(read(gui["iconTemplate"]), data)
    settings = ""
    if gui.get("settingsTemplate"):
        presets = {"factory": [{"uri": f"preset{i}", "label": p["label"]} for i, p in enumerate(model["presets"])], "user": []}
        settings = mustache(read(gui["settingsTemplate"]), {**data, "presets": presets})

    controls = model["ports"]["control"]["input"] + model["ports"]["control"]["output"]
    config = {
        "plugin": model["name"],
        "platform": platform,
        "ports": [{"symbol": p["symbol"], "index": p["index"], "default": p["default"]} for p in controls],
        "state": {str(k): v for k, v in model["state"].items()},
        "presets": [{"label": p["label"], "ports": {str(k): v for k, v in p["ports"].items()}, "state": {str(k): v for k, v in p["state"].items()}}
                    for p in model["presets"]],
    }

    document = f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<style>
{host_css()}
</style>
<style>
{css}
</style></head>
<body>
<div class="ix-board"><div class="ix-stage"><div class="mod-pedal" mod-instance="/graph/plugin">{icon}</div></div></div>
<div class="ix-window" hidden><div class="mod-settings" mod-instance="/graph/plugin">{settings}</div></div>
<script>window.__IX__ = {script_safe(json.dumps(config, ensure_ascii=False))};</script>
<script>
{script_safe((ASSETS / "vendor" / "jquery-1.9.1.min.js").read_text(encoding="utf-8"))}
</script>
<script>window.__IX_METHOD__ = (
{script_safe(read(gui["javascript"]))}
);</script>
<script>
{script_safe((ASSETS / "host.js").read_text(encoding="utf-8"))}
</script>
</body></html>
"""
    return document, model


def load_config(docs: Path, model: dict) -> dict:
    """docs/interactive.json, with the markdown of its texts turned into HTML"""
    source = docs / "interactive.json"
    if not source.is_file():
        raise SystemExit(f"{source}: this manual has an ```interactive block but no interactive.json")
    raw = json.loads(source.read_text(encoding="utf-8"))

    views = raw.get("views") or ([{"id": "pedalboard", "label": "On the pedalboard"}] +
                                 ([{"id": "settings", "label": "Settings page"}] if model["gui"].get("settingsTemplate") else []))
    areas = []
    for area in raw.get("areas", []):
        areas.append({**area, "title": inline(area["title"]), "text": markdown.markdown(area["text"]),
                      **({"chip": inline(area["chip"])} if area.get("chip") else {})})
    tours = []
    for tour in raw.get("tours", []):
        steps = [{**step, "title": inline(step.get("title", "")), "text": markdown.markdown(step["text"])} for step in tour["steps"]]
        tours.append({**tour, "steps": steps})

    return {"views": views, "areas": areas, "tours": tours, "text": {**TEXT, **raw.get("text", {})},
            "frameTitle": raw.get("frameTitle", f'{model["title"]}: the plugin, to try')}


def component(bundle: Path, docs: Path) -> str:
    """The HTML that replaces the ```interactive block"""
    document, model = frame_document(bundle)
    config = load_config(docs, model)
    return (f'<div class="ix" data-ix data-ix-loading>'
            f'<script type="application/json" data-ix-config>{json_safe(config)}</script>'
            f'<script type="application/json" data-ix-doc>{json_safe(document)}</script>'
            f'<noscript><p>The interactive manual needs JavaScript.</p></noscript></div>')


def assets() -> tuple:
    """(css, js) of the page around the frame: added to the manual only when it has an interactive block"""
    return (ASSETS / "panel.css").read_text(encoding="utf-8"), (ASSETS / "panel.js").read_text(encoding="utf-8")
