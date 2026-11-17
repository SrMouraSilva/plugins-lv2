# Display widgets: preview

Pictures of the widgets the plugins draw on the display of the **MOD Dwarf** and the **MOD Duo X**, made with the
real plugins (no device needed). The widgets use the [aLabs HMI widgets](https://github.com/sejerpz/alabs-hmi-widgets)
extension (`plugins/src/hmi/alabs-hmi-widgets.h`), which stock MOD firmware does not have: the plugins load and work
without it, they just draw nothing.

```bash
cd plugins && make build MACOS=true CFLAGS="-I/opt/homebrew/include"   # the preview loads the .so files
pip install pillow
python3 tools/hmi-preview/preview.py gallery      # <plugin>-<widget>-<n>.png: every widget on every area; the Dwarf on the left, the Duo X on the right
python3 tools/hmi-preview/preview.py devices      # overview.png: a Dwarf and a Duo X, each with several plugins at once
python3 tools/hmi-preview/preview.py animations   # overview-dwarf and overview-duox, animated (WebP and GIF)
python3 tools/hmi-preview/preview.py manual       # the pictures of each manual, in <plugin>.lv2/docs/images/: <plugin>-display.webp/.gif (animation),
                                                  # <plugin>-<widget>-<device>-<area>.webp (one for each cell of the table of areas) and -states.webp
```

The pictures go to `tools/hmi-preview/out/` (git-ignored), except the ones of the manuals, which are the ones the manuals use (rebuild them with
`python3 tools/manual/build_manual_html.py all`). The animations are lossless WebP: flat pixels, a fraction of the size of the lossy one.

## How it works

* `host.c` is a tiny LV2 host that plays the part of a device with the extension: it keeps a 1 bit framebuffer for each
  widget shown (`show <id> <symbol> <w> <h>`), runs the plugin in blocks of 128 frames and writes the framebuffers. It
  also prints a checksum of the audio and CV outputs (`hash`): compare it before and after a change to the code of a plugin.
* `session.py` builds it, reads the ports from the TTL (`../modgui-preview/ttl.py`) and drives it.
* `preview.py` has the areas of the two devices and the scenes, and draws the pictures. The sizes and positions of the
  areas are the ones the extension documents (the Dwarf's, and the Duo X's knob, two knobs, encoder band, band and knobs
  and full screen). The other Duo X areas are derived from those, and the pictures say so: check them against a
  device before trusting a position (the size of a widget is what matters; where the area sits is the device's).

## What the widgets look like (summary)

| Plugin | Widget (symbol) | Shows |
|---|---|---|
| Kill Switch | `pattern` | the pattern, the step that plays, the tempo (Depth on a tall area); dimmed and `OFF` when bypassed |
| Half Mouco | `stereo` | a row of steps for each side, or the curve of the ending along its time with a moving cursor |
| Tetr4 Switch, Hept7 Switch | `preset` | the preset that is on (number and name), what it turns on; every preset on a tall area |
| Gossiper | `footswitches` | which of the seven are on, the name of the last one that changed; only the ones assigned to a control of the device (all seven when none is), in every area |
| Gossiper | `potentiometers` | the four as bars with their value |

Each widget draws what fits in the area it is given (it never assumes a size): the footers show the bare essentials,
the full screen shows everything. `ahw:minWidth` / `ahw:minHeight` in each TTL say how small it may go.

## For whoever changes a widget

* The code is in `plugins/src/hmi/` (the canvas, the fonts, the bookkeeping of what is on screen and when to draw, and the
  widgets of Kill Switch and Half Mouco) and in `plugins/src/lv2/*/lv2-*-widgets.c` (the switches and Gossiper); the plugin's
  own file only fills in what the widget shows. A widget gives `widgets_run` a *signature* of what it draws; it is redrawn only when
  that changes, at most 20 times a second. Keep `run()` cheap: no allocation, no logging.
* `widget_shown` / `widget_hidden` come from another thread than `run()`: `hmi/widget-host.h` explains how the two meet.
* Run the sizes of every device (`preview.py gallery`) after a change, and look at them. Check with the sanitizers
  too: build the plugins and `host.c` with `-fsanitize=address,undefined` and show the widget on every area, a tiny
  one (5 x 3) and a huge one (300 x 200).
