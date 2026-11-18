# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""
Drives host.c (a tiny LV2 host that plays the part of a device with the aLabs HMI widgets) for one plugin: builds it,
connects the ports the TTL declares, and gives back what the plugin drew as Pillow images. Standard library and Pillow.
"""
import pathlib
import re
import subprocess
import sys
import tempfile

from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
PLUGINS = ROOT / "plugins"
sys.path.insert(0, str(ROOT / "tools" / "modgui-preview"))
import ttl  # noqa: E402

HOST_SOURCE = HERE / "host.c"
BUILD_DIR = pathlib.Path(tempfile.gettempdir()) / "hmi-preview-build"
HOST = BUILD_DIR / "hmi-host"


def build_host():
    BUILD_DIR.mkdir(exist_ok=True)
    if HOST.exists() and HOST.stat().st_mtime >= HOST_SOURCE.stat().st_mtime:
        return HOST
    include = ["-I/opt/homebrew/include"] if pathlib.Path("/opt/homebrew/include/lv2").is_dir() else []
    subprocess.run(["cc", "-Wall", "-Wextra", "-Wno-unused-parameter", "-O1", str(HOST_SOURCE), "-o", str(HOST), "-ldl", *include], check=True)
    return HOST


def library_path(plugin, bundles=None):
    bundle = pathlib.Path(bundles or PLUGINS / "resources") / f"{plugin}.lv2"
    for suffix in (".so", ".dylib"):
        candidate = bundle / f"{plugin}{suffix}"
        if candidate.exists():
            return candidate
    raise SystemExit(f"{plugin}: build it first (cd plugins && make build MACOS=true CFLAGS=-I/opt/homebrew/include)")


class Plugin:
    """A plugin bundle: its URI, its ports by symbol and its factory presets."""

    def __init__(self, name):
        self.name = name
        self.bundle = PLUGINS / "resources" / f"{name}.lv2"
        model = ttl.load_bundle(self.bundle)
        self.model = model
        text = (self.bundle / f"{name}.ttl").read_text()
        self.uri = re.search(r"<(http://srmourasilva\.github\.io/plugins/[a-z0-9-]+)>\s*\n\s*a lv2:Plugin", text).group(1)
        self.controls = {p["symbol"]: p for d in model["ports"]["control"].values() for p in d}
        self.audio = [(p["index"], d) for d in ("input", "output") for p in model["ports"]["audio"][d]]
        self.cv = [p["index"] for p in model["ports"]["cv"]["output"]]
        # The atom ports are not in the model: find them in the text
        self.atoms = []
        for block in re.findall(r"\[([^\[\]]*)\]", text):
            if "atom:AtomPort" in block:
                direction = "atom_in" if "lv2:InputPort" in block else "atom_out"
                self.atoms.append((int(re.search(r"lv2:index\s+(\d+)", block).group(1)), direction))
        self.preset = {p["label"]: p for p in model["presets"]}


class Session:
    """A host running one plugin. `commands` are sent when `run` is called, which returns the images asked for."""

    def __init__(self, plugin, values=None, widgets=True, bundles=None):
        self.plugin = plugin
        self.bundles = bundles
        self.lines = []
        self.dumps = []
        self.widgets = widgets
        self.values = {}
        for port in plugin.controls.values():
            self.values[port["symbol"]] = port["default"]
        self.values.update(values or {})
        for symbol, value in self.values.items():
            self.lines.append(f"control {plugin.controls[symbol]['index']} {value}")
        for index, direction in plugin.audio:
            self.lines.append(f"{'audio_in' if direction == 'input' else 'audio_out'} {index}")
        for index in plugin.cv:
            self.lines.append(f"audio_out {index}")
        for index, direction in plugin.atoms:
            self.lines.append(f"{direction} {index}")
        self.lines.append("activate")
        self.tmp = pathlib.Path(tempfile.mkdtemp(prefix="hmi-preview-"))
        self.counter = 0

    def set(self, symbol, value):
        self.lines.append(f"set {self.plugin.controls[symbol]['index']} {value}")

    def address(self, symbol, addressed=True):
        """The host tells the plugin that the port is (or no longer is) addressed to a control of the device."""
        self.lines.append(f"{'addressed' if addressed else 'unaddressed'} {self.plugin.controls[symbol]['index']}")

    def label(self, n, text):
        self.lines.append(f"label {n} {text}")

    def show(self, area_id, symbol, width, height):
        self.lines.append(f"show {area_id} {symbol} {width} {height}")

    def hide(self, area_id):
        self.lines.append(f"hide {area_id}")

    def run(self, seconds):
        self.lines.append(f"run {seconds}")

    def hash(self):
        """Queue a checksum of what the outputs wrote so far; `finish` leaves them in `self.stdout`."""
        self.lines.append("hash")

    def snap(self, area_id):
        """Queue a picture of the area; the image is in the list returned by `finish`, in order."""
        self.counter += 1
        path = self.tmp / f"snap{self.counter}.pbm"
        self.lines.append(f"dump {area_id} {path}")
        self.dumps.append(path)

    def record(self, seconds, fps, *area_ids):
        """Run for `seconds` and queue a frame of each area every 1/fps s; `finish` returns {id: [images]}."""
        prefix = self.tmp / f"rec{self.counter}_"
        self.counter += 1
        self.lines.append(f"record {prefix} {seconds} {fps} {' '.join(area_ids)}")
        self.recordings = getattr(self, "recordings", [])
        self.recordings.append((prefix, area_ids, int(seconds * fps + 0.5)))

    def finish(self):
        env = {} if self.widgets else {"HMI_NO_WIDGETS": "1"}
        library = library_path(self.plugin.name, self.bundles)
        result = subprocess.run([str(build_host()), str(library), self.plugin.uri], input="\n".join(self.lines) + "\n",
                                capture_output=True, text=True, env=env)
        if result.returncode != 0:
            raise SystemExit(f"host failed ({result.returncode}): {result.stderr}")
        self.stderr = result.stderr
        self.stdout = result.stdout
        snaps = [load_pbm(p) for p in self.dumps]
        recordings = {}
        for prefix, ids, frames in getattr(self, "recordings", []):
            for area_id in ids:
                recordings.setdefault(area_id, []).extend(load_pbm(pathlib.Path(f"{prefix}{area_id}_{f:04d}.pbm")) for f in range(frames))
        return snaps, recordings


def load_pbm(path):
    """A P4 file as an image: lit pixels white ('1' mode, 1 = lit)."""
    data = path.read_bytes()
    parts = data.split(b"\n", 2)
    width, height = map(int, parts[1].split())
    stride = (width + 7) // 8
    bits = parts[2]
    image = Image.new("1", (width, height), 0)
    pixels = image.load()
    for y in range(height):
        for x in range(width):
            if bits[y * stride + x // 8] & (0x80 >> (x % 8)):
                pixels[x, y] = 1
    return image
