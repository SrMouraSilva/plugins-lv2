#!/usr/bin/env python3
# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""
Pictures of the display widgets of the plugins on the MOD Dwarf and the MOD Duo X.

It loads the real plugins (build them first: `cd plugins && make build`) in a tiny host that plays the part of a device with
the aLabs HMI widgets extension (host.c), shows each widget on the areas those devices offer, and draws what the plugin
drew on a model of the display (128 x 64, one bit a pixel; the Duo X has two of them).

    python3 tools/hmi-preview/preview.py gallery          # every widget on every area, one picture for each plugin
    python3 tools/hmi-preview/preview.py devices          # a Dwarf and a Duo X with several plugins at once
    python3 tools/hmi-preview/preview.py animations       # animated WebP of a few of them in use
    python3 tools/hmi-preview/preview.py manual [plugin]  # the animation of each manual: docs/images/<plugin>-display.webp/.gif
    python3 tools/hmi-preview/preview.py all

The pictures go to tools/hmi-preview/out/ (git-ignored). Needs Pillow. The sizes and positions of the areas are those the
extension documents (https://github.com/sejerpz/alabs-hmi-widgets): the Dwarf's, and the Duo X's knob, two knobs, encoder band, band and
knobs, and full screen; the other Duo X areas are derived from those (a knob row is 16 high, the next one 14, the footers 8).
"""
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont

from session import Plugin, Session

OUT = pathlib.Path(__file__).resolve().parent / "out"

# --- The areas: (x, y, width, height) on a display of 128 x 64 ---------------------------------------------------------

DWARF_AREAS = {
    "col1": (2, 12, 40, 37), "col2": (44, 12, 40, 37), "col3": (86, 12, 40, 37),
    "col1-2": (2, 12, 82, 37), "col2-3": (44, 12, 82, 37), "col1-3": (2, 12, 124, 37),
    "footB": (24, 54, 53, 10), "footC": (77, 54, 51, 10), "footB-C": (24, 54, 104, 10),
    "full": (0, 0, 128, 64),
}

DUOX_AREAS = {
    "knob1": (0, 25, 64, 16), "knob2": (0, 41, 64, 14), "knob3": (64, 25, 64, 16), "knob4": (64, 41, 64, 14),
    "knobs1-2": (0, 25, 64, 30), "knobs3-4": (64, 25, 64, 30),
    "knobs1-3": (0, 25, 128, 16), "knobs2-4": (0, 41, 128, 14), "knobs": (0, 25, 128, 30),
    "enc": (0, 8, 128, 16), "enc-knobs": (0, 8, 128, 47),
    "foot1": (0, 56, 64, 8), "foot2": (64, 56, 64, 8), "foots": (0, 56, 128, 8),
    "full": (0, 0, 128, 64),
}

# (device, area, caption)
CASES = [
    ("dwarf", "col1", "Dwarf: one column"),
    ("dwarf", "col1-2", "Dwarf: two columns"),
    ("dwarf", "col1-3", "Dwarf: three columns"),
    ("dwarf", "footC", "Dwarf: one footer"),
    ("dwarf", "footB-C", "Dwarf: both footers"),
    ("dwarf", "full", "Dwarf: full screen"),
    ("duox", "knob1", "Duo X: one knob"),
    ("duox", "knobs1-2", "Duo X: two knobs, stacked"),
    ("duox", "knobs1-3", "Duo X: two knobs, side by side"),
    ("duox", "knobs", "Duo X: the four knobs"),
    ("duox", "enc-knobs", "Duo X: encoder band and knobs"),
    ("duox", "foot1", "Duo X: one footer"),
    ("duox", "foots", "Duo X: both footers"),
    ("duox", "full", "Duo X: full screen"),
]

AREAS = {"dwarf": DWARF_AREAS, "duox": DUOX_AREAS}

# --- Drawing -----------------------------------------------------------------------------------------------------------

LIT = (226, 236, 255)
UNLIT = (9, 11, 16)
OUTLINE = (44, 50, 66)
ACCENT = (255, 176, 46)
PAGE = (24, 26, 32)
TEXT = (214, 218, 228)
DIM_TEXT = (128, 134, 150)


def font(size, bold=False):
    for name in ("/System/Library/Fonts/HelveticaNeue.ttc", "/System/Library/Fonts/Helvetica.ttc",
                 "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size, index=1 if bold and name.endswith(".ttc") else 0)
        except OSError:
            continue
    return ImageFont.load_default(size)


def display(placements, scale, outlines=(), highlight=None, size=(128, 64)):
    """A display with its pixels: `placements` are (x, y, 1-bit image); `outlines` the other areas, `highlight` the one in use."""
    pixels = Image.new("1", size, 0)
    for x, y, image in placements:
        pixels.paste(image, (x, y))

    big = pixels.convert("L").resize((size[0] * scale, size[1] * scale), Image.NEAREST)
    rgb = Image.new("RGB", big.size, UNLIT)
    rgb.paste(Image.new("RGB", big.size, LIT), (0, 0), big)

    # The gaps between the pixels
    if scale >= 3:
        grid = Image.new("L", big.size, 0)
        draw = ImageDraw.Draw(grid)
        for i in range(0, big.width, scale):
            draw.line([(i, 0), (i, big.height)], fill=255, width=1)
        for j in range(0, big.height, scale):
            draw.line([(0, j), (big.width, j)], fill=255, width=1)
        rgb = Image.composite(Image.new("RGB", big.size, (0, 0, 0)), rgb, grid.point(lambda v: v * 0.55))

    draw = ImageDraw.Draw(rgb)

    def box(rect, color, width):
        x, y, w, h = rect
        draw.rectangle([x * scale - 1, y * scale - 1, (x + w) * scale, (y + h) * scale], outline=color, width=width)

    for rect in outlines:
        box(rect, OUTLINE, 1)
    if highlight:
        box(highlight, ACCENT, 2)

    return rgb


def frame(image, margin=10):
    out = Image.new("RGB", (image.width + 2 * margin, image.height + 2 * margin), (3, 4, 6))
    out.paste(image, (margin, margin))
    return out


def captioned(image, caption, sub=None):
    f = font(13, bold=True)
    s = font(11)
    probe = ImageDraw.Draw(Image.new("RGB", (1, 1)))
    # A tile is as wide as its caption needs when the picture is narrower than that
    width = max(image.width, int(probe.textlength(caption, font=f)) + 4, int(probe.textlength(sub or "", font=s)) + 4)
    tile = Image.new("RGB", (width, image.height + (40 if sub else 26)), PAGE)
    tile.paste(image, (0, 0))
    draw = ImageDraw.Draw(tile)
    draw.text((2, image.height + 5), caption, fill=TEXT, font=f)
    if sub:
        draw.text((2, image.height + 22), sub, fill=DIM_TEXT, font=s)
    return tile


def sheet(tiles, columns, title=None, subtitle=None, gap=18, margin=22):
    rows = [tiles[i:i + columns] for i in range(0, len(tiles), columns)]
    cell_w = max(t.width for t in tiles)
    cell_h = max(t.height for t in tiles)
    head = 0
    if title:
        head = 62 if subtitle else 44
    page = Image.new("RGB", (margin * 2 + columns * cell_w + (columns - 1) * gap,
                             head + margin * 2 + len(rows) * cell_h + (len(rows) - 1) * gap), PAGE)
    draw = ImageDraw.Draw(page)
    if title:
        draw.text((margin, margin - 4), title, fill=TEXT, font=font(22, bold=True))
    if subtitle:
        draw.text((margin, margin + 28), subtitle, fill=DIM_TEXT, font=font(13))
    for r, row in enumerate(rows):
        for c, tile in enumerate(row):
            page.paste(tile, (margin + c * (cell_w + gap), head + margin + r * (cell_h + gap)))
    return page


# --- The plugins --------------------------------------------------------------------------------------------------------
# For each plugin: the widgets (symbol, name) and the scenes that show them: (title, port values, preset, seconds to run)

def factory_state(plugin, preset_label):
    """The values of the ports and the labels of a factory preset of the plugin."""
    preset = plugin.preset[preset_label]
    labels = {}
    for predicate, value in preset["state"].items():
        key = str(predicate).rsplit("#", 1)[-1]
        if key.startswith(("preset_label_", "footswitch_label_")):
            labels[int(key.rsplit("_", 1)[1])] = str(value)
    return dict(preset["ports"]), labels


SCENES = {
    "kill-switch": [
        ("pattern", "Pattern", "Morello solo chop, 100 BPM", {"pattern": 6, "bpm": 100}, None, 0.37),
        ("pattern", "Pattern", "Custom (16 steps), 120 BPM, bypassed", {"pattern": 10, "bpm": 120, "lv2_enabled": 0,
                                                                       "custom_length": 16, "custom_steps_per_beat": 4,
                                                                       **{f"custom_step_{i + 1}": v for i, v in enumerate([2, 2, 0, 1, 2, 0, 0, 2, 2, 1, 0, 2, 0, 2, 2, 0])}}, None, 0.3),
    ],
    "half-mouco": [
        ("stereo", "Stereo pattern", "Seesaw, 110 BPM", {"mode": 0, "pattern": 7, "bpm": 110}, None, 0.43),
        ("stereo", "Stereo pattern", "Ending: bouncing ball, 8 beats at 120 BPM (a second in)", {"mode": 1, "ending": 0, "bpm": 120, "length": 8}, None, 1.0),
        ("stereo", "Stereo pattern", "Ending: helicopter, 16 beats at 120 BPM (3 s in)", {"mode": 1, "ending": 1, "bpm": 120, "length": 16}, None, 3.0),
    ],
    "tetr4-switch": [
        ("preset", "Active preset", "Four-scene guitar rig, preset 3 on", None, ("Four-scene guitar rig", 2), 0.2),
    ],
    "hept7-switch": [
        ("preset", "Active preset", "factory preset, preset 3 on", None, ("__first__", 2), 0.2),
    ],
    "gossiper": [
        ("footswitches", "Footswitches", "switches 1, 4 and 6 on; 6 pressed last", {"__switches__": [1, 4, 6]}, None, 0.2),
        ("footswitches", "Footswitches", "switches 1, 4 and 6 on; only 4 and 6 are assigned to controls of the device (every area shows only those)", {"__switches__": [1, 4, 6], "__addressed__": [4, 6]}, None, 0.2),
        ("potentiometers", "Potentiometers", "four knobs", {"__pots__": [0.82, 0.35, 0.0, 1.0]}, None, 0.2),
    ],
}


def load_scene(plugin, spec):
    """The session for a scene, with the ports and the labels set, but not run."""
    symbol, name, subtitle, values, preset, seconds = spec
    values = dict(values or {})
    labels = {}

    if preset is not None:
        label, selected = preset
        if label == "__first__":
            label = sorted(plugin.preset)[0]
        values, labels = factory_state(plugin, label)
        for n in range(1, 8):
            if f"Preset_selector_{n}" in plugin.controls:
                values[f"Preset_selector_{n}"] = 1.0 if n - 1 == selected else 0.0
        subtitle = f"{label}, preset {selected + 1} on"

    switches = values.pop("__switches__", None)
    addressed = values.pop("__addressed__", None)
    pots = values.pop("__pots__", None)
    if switches is not None:
        for n in range(1, 8):
            values[f"Footswitch_{n}"] = 1.0 if n in switches else 0.0
    if pots is not None:
        for n, v in enumerate(pots):
            values[f"Potentiometer_{n + 1}"] = v

    values = dict(values)
    if addressed:
        values["__addressed__"] = addressed
    return symbol, name, subtitle, values, labels, seconds


def widget_pictures(plugin, spec, cases):
    """What the widget draws on each of the cases (a list of (device, area, caption)): one host for each, as a device
    shows a widget on a few areas at a time (WIDGET_MAX_SHOWINGS)."""
    symbol, name, subtitle, values, labels, seconds = load_scene(plugin, spec)
    snaps = []

    for device, area, caption in cases:
        x, y, w, h = AREAS[device][area]
        session = Session(plugin, {k: v for k, v in values.items() if k != "__addressed__"})

        for n, text in labels.items():
            session.label(n, text)

        for number in values.get("__addressed__", []):
            session.address(f"Footswitch_{number}")

        session.show("a", symbol, w, h)
        session.run(seconds)
        session.snap("a")
        snaps.append(session.finish()[0][0])

    return name, subtitle, snaps


DWARF_COLOR = (112, 170, 255)
DUOX_COLOR = (118, 214, 160)


def section(tiles, columns, title, subtitle, color):
    """A block of tiles under a title with a bar in the colour of the device: a sheet of one device."""
    block = sheet(tiles, columns, title, subtitle)
    ImageDraw.Draw(block).rectangle([0, 0, block.width, 5], fill=color)
    return block


def gallery(plugins):
    """One sheet for each plugin and scene: the Dwarf's areas on the left, the Duo X's on the right, each under its own title."""
    OUT.mkdir(exist_ok=True)

    for name in plugins:
        plugin = Plugin(name)

        for number, spec in enumerate(SCENES[name], 1):
            widget_title, subtitle, snaps = widget_pictures(plugin, spec, CASES)
            symbol = spec[0]
            tiles = {"dwarf": [], "duox": []}

            for (device, area, caption), snap in zip(CASES, snaps):
                rect = AREAS[device][area]
                others = [r for key, r in AREAS[device].items() if r != rect and not key.startswith("full") and r[2] * r[3] < 5000 and (device == "dwarf" or key in ("knob1", "knob2", "knob3", "knob4", "enc", "foot1", "foot2"))]
                image = frame(display([(rect[0], rect[1], snap)], 3, others, rect))
                tiles[device].append(captioned(image, caption.split(": ", 1)[1], f"{rect[2]} x {rect[3]}"))

            left = section(tiles["dwarf"], 2, "MOD Dwarf", "One display of 128 x 64 pixels", DWARF_COLOR)
            right = section(tiles["duox"], 2, "MOD Duo X", "Two displays of 128 x 64 pixels (each area exists on both)", DUOX_COLOR)

            head = 84
            gap = 28
            page = Image.new("RGB", (left.width + gap + right.width + 44, head + max(left.height, right.height) + 22), PAGE)
            draw = ImageDraw.Draw(page)
            draw.text((22, 18), f"{name}: {widget_title}", fill=TEXT, font=font(26, bold=True))
            draw.text((22, 54), subtitle, fill=DIM_TEXT, font=font(14))
            page.paste(left, (22, head))
            page.paste(right, (22 + left.width + gap, head))
            target = OUT / f"{name}-{symbol}-{number}.png"
            page.save(target)
            print(target)


# --- The devices ----------------------------------------------------------------------------------------------------------
# A Dwarf and a Duo X with several plugins on their displays, at one moment and as animations.

FPS = 15


def scene_values(plugin, spec_index):
    return load_scene(plugin, SCENES[plugin.name][spec_index])


def frames_of(plugin_name, spec_index, area_size, script, override=None):
    """The frames of a widget of `area_size` while `script(session, record)` drives the plugin; `record(seconds)` runs and takes frames."""
    plugin = Plugin(plugin_name)
    symbol, _, _, values, labels, _ = scene_values(plugin, spec_index)
    session = Session(plugin, {**values, **(override or {})})

    for n, text in labels.items():
        session.label(n, text)

    session.show("a", symbol, *area_size)
    script(session, lambda seconds: session.record(seconds, FPS, "a"))
    return session.finish()[1]["a"]


def device_image(device, layers, scale):
    """One display or two (the Duo X): `layers` is a list of (display index, area, image)."""
    displays = 2 if device == "duox" else 1
    rendered = []

    for d in range(displays):
        areas = AREAS[device]
        placements = [(areas[area][0], areas[area][1], image) for index, area, image in layers if index == d]
        used = [areas[area] for index, area, image in layers if index == d]
        others = [r for key, r in areas.items() if r not in used and not key.startswith("full")]
        rendered.append(frame(display(placements, scale, others, size=(128, 64))))

    if displays == 1:
        return rendered[0]

    gap = 14
    body = Image.new("RGB", (rendered[0].width * 2 + gap, rendered[0].height), (3, 4, 6))
    body.paste(rendered[0], (0, 0))
    body.paste(rendered[1], (rendered[0].width + gap, 0))
    return body


def with_caption(image, title, subtitle):
    page = Image.new("RGB", (image.width + 44, image.height + 96), PAGE)
    draw = ImageDraw.Draw(page)
    draw.text((22, 16), title, fill=TEXT, font=font(20, bold=True))
    draw.text((22, 44), subtitle, fill=DIM_TEXT, font=font(13))
    page.paste(image, (22, 74))
    return page


# The animations start on preset 1
TETR4_FIRST = {"Preset_selector_1": 1.0, "Preset_selector_2": 0.0, "Preset_selector_3": 0.0, "Preset_selector_4": 0.0}


def tetr4_script(s, record):
    # Footswitch selectors behave as radio buttons: the one that turns on is the preset
    record(0.8)
    for step, number in enumerate((2, 3, 4, 1)):
        previous = (1, 2, 3, 4)[step]
        s.set(f"Preset_selector_{previous}", 0.0)
        s.set(f"Preset_selector_{number}", 1.0)
        record(0.8)


def gossiper_script(s, record):
    record(0.6)
    for number in (1, 4, 6, 4):
        s.set(f"Footswitch_{number}", 0.0 if s.values.get(f"Footswitch_{number}", 0.0) else 1.0)
        s.values[f"Footswitch_{number}"] = 0.0 if s.values.get(f"Footswitch_{number}", 0.0) else 1.0
        record(0.9)


def devices(animated=False):
    OUT.mkdir(exist_ok=True)
    scale = 4

    # --- Dwarf
    kill = frames_of("kill-switch", 0, DWARF_AREAS["col1-2"][2:], lambda s, record: record(4.0))
    mouco = frames_of("half-mouco", 1, DWARF_AREAS["col3"][2:], lambda s, record: record(4.0))
    tetr4 = frames_of("tetr4-switch", 0, DWARF_AREAS["footB-C"][2:], tetr4_script, TETR4_FIRST)
    count = min(len(kill), len(mouco), len(tetr4))
    dwarf = [device_image("dwarf", [(0, "col1-2", kill[i]), (0, "col3", mouco[i]), (0, "footB-C", tetr4[i])], scale) for i in range(count)]

    # --- Duo X: the left display with Kill Switch and Tetr4, the right one with Half Mouco and Gossiper
    kill_x = frames_of("kill-switch", 0, DUOX_AREAS["enc-knobs"][2:], lambda s, record: record(4.0))
    tetr4_x = frames_of("tetr4-switch", 0, DUOX_AREAS["foots"][2:], tetr4_script, TETR4_FIRST)
    mouco_x = frames_of("half-mouco", 1, DUOX_AREAS["enc-knobs"][2:], lambda s, record: record(4.0))
    goss_x = frames_of("gossiper", 0, DUOX_AREAS["foots"][2:], gossiper_script)
    count_x = min(len(kill_x), len(tetr4_x), len(mouco_x), len(goss_x))
    duox = [device_image("duox", [(0, "enc-knobs", kill_x[i]), (0, "foots", tetr4_x[i]), (1, "enc-knobs", mouco_x[i]), (1, "foots", goss_x[i])], scale) for i in range(count_x)]

    pictures = {
        "dwarf": (with_caption(dwarf[0], "MOD Dwarf", "Kill Switch (two columns), Half Mouco (one column, an ending) and Tetr4 Switch (both footers)"), dwarf),
        "duox": (with_caption(duox[0], "MOD Duo X", "Left display: Kill Switch (encoder band and knobs) and Tetr4 Switch (both footers). Right display: Half Mouco and Gossiper"), duox),
    }

    # The still picture is a frame where the interesting things happen: the middle of the animation. The two devices one
    # above the other, each under its own title
    middles = [with_caption(frames[len(frames) // 2], still_title(name), still_subtitle(name)) for name, (still, frames) in pictures.items()]
    stacked = Image.new("RGB", (max(m.width for m in middles), sum(m.height for m in middles)), PAGE)
    y = 0
    for middle, color in zip(middles, (DWARF_COLOR, DUOX_COLOR)):
        stacked.paste(middle, (0, y))
        ImageDraw.Draw(stacked).rectangle([0, y, stacked.width, y + 5], fill=color)
        y += middle.height
    stacked.save(OUT / "overview.png")
    print(OUT / "overview.png")

    if animated:
        for name, (still, frames) in pictures.items():
            animate([with_caption(f, still_title(name), still_subtitle(name)) for f in frames], OUT / f"overview-{name}")


def still_title(name):
    return "MOD Dwarf" if name == "dwarf" else "MOD Duo X"


def still_subtitle(name):
    if name == "dwarf":
        return "Kill Switch (two columns), Half Mouco (one column, an ending) and Tetr4 Switch (both footers)"
    return "Left display: Kill Switch (encoder band and knobs) and Tetr4 Switch (both footers). Right display: Half Mouco and Gossiper"


def animate(frames, target):
    """An endless animation as WebP and GIF (the GIF for what cannot play WebP)."""
    duration = int(1000 / FPS)
    frames[0].save(f"{target}.webp", save_all=True, append_images=frames[1:], duration=duration, loop=0, lossless=True, method=4)
    palette = [f.convert("P", palette=Image.ADAPTIVE, colors=64) for f in frames]
    palette[0].save(f"{target}.gif", save_all=True, append_images=palette[1:], duration=duration, loop=0, optimize=True)
    print(f"{target}.webp", (pathlib.Path(f"{target}.webp").stat().st_size) // 1024, "KB;", f"{target}.gif", (pathlib.Path(f"{target}.gif").stat().st_size) // 1024, "KB")


# --- The manuals ------------------------------------------------------------------------------------------------------------
# One animation for each plugin, the Dwarf and the Duo X side by side: plugins/resources/<name>.lv2/docs/images/<name>-display.webp/.gif

BUNDLES = pathlib.Path(__file__).resolve().parents[2] / "plugins" / "resources"


def frames_multi(plugin_name, spec_index, placements, script, override=None):
    """The frames of several widgets of one plugin: `placements` are (id, symbol, width, height). Returns {id: [images]}."""
    plugin = Plugin(plugin_name)
    symbol, _, _, values, labels, _ = scene_values(plugin, spec_index)
    addressed = values.pop("__addressed__", [])
    session = Session(plugin, {**values, **(override or {})})
    ids = [p[0] for p in placements]

    for n, text in labels.items():
        session.label(n, text)
    for number in addressed:
        session.address(f"Footswitch_{number}")
    for area_id, widget_symbol, w, h in placements:
        session.show(area_id, widget_symbol, w, h)

    script(session, lambda seconds: session.record(seconds, FPS, *ids))
    return session.finish()[1]


def kill_script(s, record):
    # On for a while, then bypassed: the widget goes grey and says OFF
    record(3.5)
    s.set("lv2_enabled", 0)
    record(1.5)


def mouco_script(s, record):
    # The patterns, then the ending (switching Mode starts it from the beginning)
    record(2.0)
    s.set("mode", 1)
    record(4.4)


def switch_script(sequence):
    """The presets of a switch one after the other, as footswitches would choose them (the selectors are radio buttons)."""
    def script(s, record):
        record(0.9)
        previous = 1
        for number in sequence:
            s.set(f"Preset_selector_{previous}", 0.0)
            s.set(f"Preset_selector_{number}", 1.0)
            previous = number
            record(0.9)
    return script


def gossiper_both_script(s, record):
    import math
    toggles = {10: 1, 22: 4, 34: 6, 46: 1, 58: 4, 66: 6}
    on = set()
    for step in range(75):
        if step in toggles:
            number = toggles[step]
            on ^= {number}
            s.set(f"Footswitch_{number}", 1.0 if number in on else 0.0)
        for i in range(4):
            s.set(f"Potentiometer_{i + 1}", 0.5 + 0.5 * math.sin(2 * math.pi * (step / 75.0) + i * 1.3))
        record(1.0 / FPS)


# plugin: (scene, script, ports to start with, widgets, Dwarf areas, Duo X areas, captions)
MANUAL = {
    "kill-switch": dict(spec=0, script=kill_script, override={}, dwarf=[("col1-2", "pattern")], duox=[("enc-knobs", "pattern"), ("foots", "pattern")],
                        dwarf_text="Two columns", duox_text="Encoder band and knobs, and both footers"),
    "half-mouco": dict(spec=0, script=mouco_script, override={"length": 8, "ending": 0}, dwarf=[("col1-2", "stereo"), ("col3", "stereo")],
                       duox=[("enc-knobs", "stereo")], dwarf_text="Two columns and one column", duox_text="Encoder band and knobs"),
    "tetr4-switch": dict(spec=0, script=switch_script((2, 3, 4, 1)), override=TETR4_FIRST, dwarf=[("col1", "preset"), ("footB-C", "preset")],
                         duox=[("knobs1-2", "preset"), ("foots", "preset")], dwarf_text="One column and both footers", duox_text="Two knobs and both footers"),
    "hept7-switch": dict(spec=0, script=switch_script((2, 3, 5, 7, 1)), override={**TETR4_FIRST, "Preset_selector_5": 0.0, "Preset_selector_6": 0.0, "Preset_selector_7": 0.0},
                         dwarf=[("col1", "preset"), ("footB-C", "preset")], duox=[("knobs1-2", "preset"), ("foots", "preset")],
                         dwarf_text="One column and both footers", duox_text="Two knobs and both footers"),
    "gossiper": dict(spec=0, script=gossiper_both_script, override={**{f"Footswitch_{n}": 0.0 for n in range(1, 8)}},
                     dwarf=[("col1-2", "footswitches"), ("col3", "potentiometers")], duox=[("knobs", "footswitches"), ("foots", "potentiometers")],
                     dwarf_text="Footswitches in two columns, knobs in one", duox_text="Footswitches in the four knobs, knobs in both footers"),
}


def panel(image, title, text):
    page = Image.new("RGB", (image.width + 24, image.height + 70), PAGE)
    draw = ImageDraw.Draw(page)
    draw.text((12, 10), title, fill=TEXT, font=font(17, bold=True))
    draw.text((12, 36), text, fill=DIM_TEXT, font=font(12))
    page.paste(image, (12, 58))
    return page


def manual_animation(name):
    config = MANUAL[name]
    placements = []

    for kind in ("dwarf", "duox"):
        for index, (area, symbol) in enumerate(config[kind]):
            x, y, w, h = AREAS[kind][area]
            placements.append((f"{kind}{index}", symbol, w, h))

    frames = frames_multi(name, config["spec"], placements, config["script"], config["override"])
    count = min(len(v) for v in frames.values())
    scale = 3
    images = []

    for i in range(count):
        dwarf = device_image("dwarf", [(0, area, frames[f"dwarf{k}"][i]) for k, (area, symbol) in enumerate(config["dwarf"])], scale)
        duox = device_image("duox", [(0, area, frames[f"duox{k}"][i]) for k, (area, symbol) in enumerate(config["duox"])], scale)
        # The Duo X has two displays: one is enough to show a widget
        duox = duox.crop((0, 0, dwarf.width, duox.height))
        left = panel(dwarf, "MOD Dwarf", config["dwarf_text"])
        right = panel(duox, "MOD Duo X (one of its two displays)", config["duox_text"])
        page = Image.new("RGB", (left.width + right.width + 16, left.height), PAGE)
        ImageDraw.Draw(page).rectangle([0, 0, left.width - 1, 3], fill=DWARF_COLOR)
        ImageDraw.Draw(page).rectangle([left.width + 16, 0, page.width, 3], fill=DUOX_COLOR)
        page.paste(left, (0, 0))
        page.paste(right, (left.width + 16, 0))
        images.append(page)

    target = BUNDLES / f"{name}.lv2" / "docs" / "images" / f"{name}-display"
    animate(images, target)


# --- Every area and every state, for the manuals -----------------------------------------------------------------------------
# For each widget: its picture on every area that offers it (from the minimum its TTL declares), one for each cell of the table of areas
# of its manual, and its states side by side: plugins/resources/<name>.lv2/docs/images/<plugin>-<widget>-<device>-<area>.webp and -states.webp

def widget_minimums(plugin_name):
    """{symbol: (name, min width, min height)} of the `ahw:widget`s of the TTL of the plugin."""
    import re
    text = (BUNDLES / f"{plugin_name}.lv2" / f"{plugin_name}.ttl").read_text()
    found = {}
    for block in re.findall(r"\[\s*a ahw:Widget ;(.*?)\n\s*\]", text, re.S):
        symbol = re.search(r'lv2:symbol "([^"]+)"', block).group(1)
        name = re.search(r'lv2:name "([^"]+)"', block).group(1)
        width = int(re.search(r"ahw:minWidth (\d+)", block).group(1))
        height = int(re.search(r"ahw:minHeight (\d+)", block).group(1))
        found[symbol] = (name, width, height)
    return found


def save_still(image, target):
    image.save(target, "WEBP", lossless=True, method=4)
    print(target, pathlib.Path(target).stat().st_size // 1024, "KB")


# The rows of the table of areas of a manual: a kind of area, the area of the Dwarf and the one of the Duo X that go with it (the
# pairs of the extension's own table of areas; None: the device has none of that kind). The Duo X's own knob areas have no Dwarf pair
ROWS = [
    ("A footer", "footC", "foot1"),
    ("Both footers", "footB-C", "foots"),
    ("A column / two knobs one above the other", "col1", "knobs1-2"),
    ("Two columns / the four knobs", "col1-2", "knobs"),
    ("Three columns / the encoder band and the knobs", "col1-3", "enc-knobs"),
    ("The whole screen", "full", "full"),
    ("One knob", None, "knob1"),
    ("Two knobs side by side", None, "knobs1-3"),
]

# (plugin, widget): the scene (an index of SCENES) the areas are drawn with
AREA_SCENE = {
    ("kill-switch", "pattern"): 0, ("half-mouco", "stereo"): 0, ("tetr4-switch", "preset"): 0, ("hept7-switch", "preset"): 0,
    ("gossiper", "footswitches"): 1, ("gossiper", "potentiometers"): 2,
}

# The states: (caption, scene, (width, height)); the scene is a tuple like those of SCENES
STATES = {
    ("kill-switch", "pattern"): [
        ("Playing", ("pattern", "", "", {"pattern": 6, "bpm": 100}, None, 0.37), (82, 37)),
        ("Your own pattern (16 steps)", ("pattern", "", "", {"pattern": 10, "bpm": 120, "custom_length": 16, "custom_steps_per_beat": 4,
                                                           **{f"custom_step_{i + 1}": v for i, v in enumerate([2, 2, 0, 1, 2, 0, 0, 2, 2, 1, 0, 2, 0, 2, 2, 0])}}, None, 0.3), (82, 37)),
        ("Too fast to follow: a dotted line", ("pattern", "", "", {"pattern": 3, "bpm": 160}, None, 0.3), (82, 37)),
        ("Bypassed: grey, OFF", ("pattern", "", "", {"pattern": 6, "bpm": 100, "lv2_enabled": 0}, None, 0.3), (82, 37)),
    ],
    ("half-mouco", "stereo"): [
        ("A pattern", ("stereo", "", "", {"mode": 0, "pattern": 7, "bpm": 110}, None, 0.43), (82, 37)),
        ("Ending: bouncing ball, one second in", ("stereo", "", "", {"mode": 1, "ending": 0, "bpm": 120, "length": 8}, None, 1.0), (82, 37)),
        ("Ending: helicopter, three seconds in", ("stereo", "", "", {"mode": 1, "ending": 1, "bpm": 120, "length": 16}, None, 3.0), (82, 37)),
        ("The ending is over", ("stereo", "", "", {"mode": 1, "ending": 0, "bpm": 120, "length": 2}, None, 1.5), (82, 37)),
        ("Bypassed: grey, OFF", ("stereo", "", "", {"mode": 0, "pattern": 7, "bpm": 110, "lv2_enabled": 0}, None, 0.3), (82, 37)),
    ],
    ("tetr4-switch", "preset"): [
        (f"Preset {n + 1} on", ("preset", "", "", None, ("Four-scene guitar rig", n), 0.2), (124, 37)) for n in range(4)
    ],
    ("hept7-switch", "preset"): [
        ("Preset 1 on", ("preset", "", "", None, ("__first__", 0), 0.2), (128, 64)),
        ("Preset 5 on", ("preset", "", "", None, ("__first__", 4), 0.2), (128, 64)),
    ],
    ("gossiper", "footswitches"): [
        ("None on", ("footswitches", "", "", {"__switches__": []}, None, 0.2), (82, 37)),
        ("1, 4 and 6 on; 6 changed last", ("footswitches", "", "", {"__switches__": [1, 4, 6]}, None, 0.2), (82, 37)),
        ("A column, none assigned: all seven", ("footswitches", "", "", {"__switches__": [1, 4, 6]}, None, 0.2), (40, 37)),
        ("A column, only 4 and 6 assigned", ("footswitches", "", "", {"__switches__": [1, 4, 6], "__addressed__": [4, 6]}, None, 0.2), (40, 37)),
    ],
    ("gossiper", "potentiometers"): [
        ("Four knobs at rest", ("potentiometers", "", "", {"__pots__": [0.0, 0.0, 0.0, 0.0]}, None, 0.2), (82, 37)),
        ("Turned", ("potentiometers", "", "", {"__pots__": [0.82, 0.35, 0.0, 1.0]}, None, 0.2), (82, 37)),
        ("A column", ("potentiometers", "", "", {"__pots__": [0.82, 0.35, 0.0, 1.0]}, None, 0.2), (40, 37)),
    ],
}


def snapshot(plugin, spec, size):
    symbol, _, _, values, labels, seconds = load_scene(plugin, spec)
    session = Session(plugin, {k: v for k, v in values.items() if k != "__addressed__"})
    for n, text in labels.items():
        session.label(n, text)
    for number in values.get("__addressed__", []):
        session.address(f"Footswitch_{number}")
    session.show("a", symbol, *size)
    session.run(seconds)
    session.snap("a")
    return session.finish()[0][0]


def manual_stills(name):
    plugin = Plugin(name)
    images = BUNDLES / f"{name}.lv2" / "docs" / "images"

    for symbol, (widget_name, min_w, min_h) in widget_minimums(name).items():
        spec = SCENES[name][AREA_SCENE[(name, symbol)]]

        # Every area that offers the widget: one picture for each cell of the table of its manual (a row is a kind of area, a column a
        # device), the widget on the display with its area marked. A cell of an area too small for the widget has no picture
        for device, areas in (("dwarf", [r[1] for r in ROWS]), ("duox", [r[2] for r in ROWS])):
            for area in areas:
                if area is None or AREAS[device][area][2] < min_w or AREAS[device][area][3] < min_h:
                    continue
                _, _, snaps = widget_pictures(plugin, spec, [(device, area, "")])
                rect = AREAS[device][area]
                # The outlines of the other areas, but not the ones that run across the widget (they would cut its picture)
                others = [r for key, r in AREAS[device].items() if not key.startswith("full") and r[2] * r[3] < 5000
                          and (device == "dwarf" or key in ("knob1", "knob2", "knob3", "knob4", "enc", "foot1", "foot2"))
                          and (r[0] + r[2] <= rect[0] or rect[0] + rect[2] <= r[0] or r[1] + r[3] <= rect[1] or rect[1] + rect[3] <= r[1])]
                save_still(frame(display([(rect[0], rect[1], snaps[0])], 2, others, rect), 4), images / f"{name}-{symbol}-{device}-{area}.webp")

        # The states, side by side
        tiles = []
        for caption, state_spec, size in STATES[(name, symbol)]:
            snap = snapshot(plugin, state_spec, size)
            tiles.append(captioned(frame(display([(0, 0, snap)], 3 if size[0] < 100 else 2, size=size), 8), caption, f"{size[0]} x {size[1]} pixels"))
        block = sheet(tiles, 2 if len(tiles) > 3 else len(tiles), gap=14, margin=14)
        labelled = Image.new("RGB", (block.width, block.height + 34), PAGE)
        ImageDraw.Draw(labelled).text((14, 10), f"{widget_name}: what it shows", fill=TEXT, font=font(15, bold=True))
        labelled.paste(block, (0, 34))
        save_still(labelled, images / f"{name}-{symbol}-states.webp")


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "gallery"
    targets = [t for t in sys.argv[2:] if t in SCENES] or list(SCENES)

    if what in ("gallery", "all"):
        gallery(targets)
    if what == "devices":
        devices()
    if what in ("animations", "all"):
        devices(animated=True)
    if what in ("manual", "all"):
        for name in (sys.argv[2:] or MANUAL):
            manual_animation(name)
            manual_stills(name)
