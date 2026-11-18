# CLAUDE.md

LV2 utility plugins for MOD devices (MOD Dwarf first; Duo and Duo X must keep working). Plugins: **Gossiper**,
**Tetr4 Switch**, **Hept7 Switch** and **Kill Switch**. Code, comments, commits and docs are in English.

## Layout

```
plugins/src/                 C sources (shared code in lv2/, model/, utils/; kill-switch.c is standalone)
plugins/resources/<name>.lv2 the bundle: <name>.ttl, manifest.ttl, modgui.ttl, presets.ttl, modgui/, docs/
plugins/Makefile             build-<plugin> targets; srmourasilva-plugins.mk is the mod-plugin-builder package
tools/modgui-preview/        browser preview of any plugin's modgui (no device needed)
tools/hmi-preview/           pictures of the display widgets on the Dwarf and the Duo X (real plugins in a tiny host); see "Display widgets"
tools/media/                 animations (animated WebP + GIF) and alternative thumbnails (from the preview); see "Animations and alternative thumbnails"
tools/manual/                builds modgui/manual.html from docs/manual.md; see "HTML manuals" (tools/manual/interactive/: the plugin inside the manual)
docs/                        guides for other developers: custom-settings-pages, making-animations, alternative-thumbnails, html-manuals, interactive-manuals
.github/workflows/release.yml  builds Duo, Duo X and Dwarf; PLUGINS lists what is packaged
```

New files carry `Copyright 2021-2026 Paulo Mateus` and `SPDX-License-Identifier: Apache-2.0`. Third-party code keeps
its own license and is listed in `NOTICE`.

## Build and verify

```bash
cd plugins && make build                       # Linux
make build MACOS=true CFLAGS="-I/opt/homebrew/include"   # macOS (homebrew lv2); both switch plugins and gossiper
```

* The build must have **no warnings**, and always build **all** plugins: they share headers (`config.h` is also
  included by Gossiper; a macro of the same name in two plugins is a warning, e.g. `PLUGIN_URI`, which is why the
  switches use `SWITCH_PLUGIN_URI`). `*.so` is git-ignored: delete what you built before committing.
* There are no automated tests. When you change C code, check it by loading the `.so` in a tiny host (`dlopen`,
  urid map, the change-request feature, optionally a fake `WidgetControl`), and **compare the output traces
  before and after** a refactor (build the previous commit with `git archive`). Do this for every plugin that
  shares the code you touched.
* Check the TTL with `lv2info <uri>` (`LV2_PATH=plugins/resources`) and, better, with MOD UI's own reader:
  `from modtools.utils import init, get_plugin_info` (needs a mod-ui checkout with `libmod_utils.so`). `lv2_validate`
  complains about `lv2:majorVersion` and the HMI URIs; that noise is expected.
* **Testing on a Dwarf** (SSH as root; the plugins are in `/root/.lv2`): build the bundles with the Docker image
  (`mpb`, see the Readme), take them out with `docker cp`, pack them with `COPYFILE_DISABLE=1 tar` (otherwise macOS adds `._*`
  files that make lilv print errors), copy with `scp -O` and swap each bundle directory with `mv` (never overwrite a loaded
  `.so`). Restart `mod-ui`, and `jack2` first when ports changed (see above). The firmware's `addressed` calls show in
  `journalctl -b | grep mod-jackd`. Say that it was tested there, or that it was not.
* Look at the interface with `python3 tools/modgui-preview/serve.py` (see its README). It does not replace a test
  on a real pedalboard or device: say so when you could not test there.

## Adding a plugin (checklist)

1. Bundle in `plugins/resources/<name>.lv2` (TTLs, `modgui/`). Port symbols and parameter URIs are saved inside
   users' pedalboards: **never rename or renumber them after a release**; add new ones at the end.
2. `plugins/Makefile`: `build-<name>`, and add it to `build`, `clean`, `install`, `install-user`, `uninstall-user`.
3. `plugins/srmourasilva-plugins.mk`: add `<name>.lv2` to `SRMOURASILVA_PLUGINS_BUNDLES` and **bump
   `SRMOURASILVA_PLUGINS_VERSION`** (otherwise the builder does not rebuild). Add `install-<name>` to the Makefile and a recipe
   `store/<name>.mk` (copy another one: the MOD store takes **one bundle per package**), then run `python3 tools/store/pin.py`.
4. `.github/workflows/release.yml`: add it to `PLUGINS`.
5. `Readme.md` section, `NOTICE` title, a user manual (below), screenshot and thumbnail, the two animations and the alternative
   thumbnail (and the plugin in `PLUGINS` of `tools/site/build_site.py`, so it is on the documentation site) (section "Animations and alternative thumbnails": add the plugin to `tools/media/scenarios.mjs` and `thumbnails.mjs`).

## C code and LV2

* **Kill Switch is one standalone file** (`src/kill-switch.c`, only LV2's core header; its display widget is apart, in `src/hmi/kill-switch-widgets.c`, which only needs the headers of `src/hmi/` that every plugin shares, and draws the endings with the plugin's own `ending_gate`, handed to it).
  It is stereo: a row of steps for each side, plus *endings*, one-shot effects that are functions of the time since the effect was switched on
  (`ending_gate` in C, `ENDINGS[].gate` in the modgui script: keep them in step, the script draws the ending along its time). A step has three levels
  ('0' cut, '1' half, '2' pass). `PATTERNS` (the first ten have the same row on both sides: the single-channel kill switch; then nine with a row for each
  side; then Custom, value 19 of `pattern`), the scale points of `pattern` and `ending` and the modgui script must agree. The custom pattern is the ports
  `custom_length`, `custom_steps_per_beat`, `custom_left_step_1..16` and `custom_right_step_1..16`. The ports are `mode`, `pattern`, `ending`, ...,
  and `kill` last (index 47). A new port goes before `kill` only while the plugin is unreleased (the TTL indices, `PortIndex` and the harness must agree).
* **The on/off is the `kill` port, not `lv2:enabled`**: MOD UI draws `lv2:enabled` as its own `:bypass`, a made-up port whose properties are fixed
  (`toggled`, `integer`), so a `mod:preferMomentaryOnByDefault` on it never reaches the assignment dialog; a plain toggled port with that property does,
  and the dialog then comes as *Momentary*. It is off by default; the effect (the pattern or the ending) starts from its first step each time it goes on,
  and goes back to the clean signal with a 10 ms ramp. MOD's own bypass of the plugin is still the host's. The pedal's footswitch and the settings page's
  *Kill* button write `kill` themselves (a tap toggles it).
* **One code base, several plugins.** Tetr4 and Hept7 Switch are the same sources sized by `src/config.h`
  (`-DHEPT7_SWITCH`). Do not copy files to make a variant: parametrise (`TOTAL_PRESETS`, `TOTAL_OUTPUTS`).
* **Port layout lives in `src/model/ports.h`** as contiguous ranges (`PORT_*_FIRST`), never as one `case` per port.
  The TTL indices must match it; `_Static_assert`s pin the Tetr4 layout. Changing a port means changing, together:
  `ports.h`, the TTL (indices, groups), `presets.ttl`, the modgui JS, the manual.
* Keep `run()` cheap and real-time friendly: no logging per cycle, no unbounded work, no allocations (the switches
  keep their labels in fixed buffers; Gossiper still allocates when it applies a label from a `patch:Set`: do not add
  more). Port values the plugin sets itself (selector radio behaviour, the combobox) go through
  `control_input_port->request_change`, which may only be called from `run()`; never write into an input port. mod-host
  ignores a request for the value a port already holds, and writes the port at once otherwise.
* **The HMI is optional** (`LV2_HMI__WidgetControl` is `false` in `lv2_features_query` and `optionalFeature` in the
  TTL): the MOD Duo has none, and the plugin must still load and work. Guard every use of `widgetControl`.
  The HMI has seven LED colours (`LV2_HMI_LED_Colour_*`): a preset-per-footswitch plugin can have at most seven distinct ones.
* **Talk to the HMI as little as possible, and not from `run()`.** mod-host locks a mutex and writes to the HMI channel
  for every LED, label and pop-up. The switches send only what a change of preset touches, only when the preset really
  changes (a momentary footswitch being released is not a change), and from the LV2 worker (`work:schedule`, optional:
  without it, from `run()`). `addressed()` shows the state on that one widget and never pops up.
* **Hept7 has three buttons where Tetr4 has one enumeration**: `Current preset` (the port `Preset_combobox`, index 15, which
  loops forward), `Next preset` and `Previous preset`. All three are `lv2:toggled` with `mod:preferMomentaryOffByDefault` and
  default 1, and they act when released (the port goes 0 to 1: `Controller.button_mask`, `pressed`). A step is taken in the
  cycle (`Preset_N_in_cycle`) from the **active preset**, so nothing depends on where the device is. Why not an enumeration: the
  firmware keeps its own step, draws the text of the value from the `scalePoint` of that step, refuses `set_value` on a
  footswitch (`cb_change_assigment_value`), and only MOD UI's web side (`paramhmi_set`) tells it a new value, never a write
  by the plugin; the B+C pair is one control per footswitch, each with its own label, of which mod-host hands the plugin only one
  addressing. The B&C actuator accepts only `enumeration` and `colouredlist` (`/etc/mod-hardware-descriptor.json`), so the
  buttons cannot be assigned to it, and nothing in MOD UI keeps a toggled port off a knob (it then works as a button).
  Tetr4 keeps the enumeration (its scale points are "1" to "4": a one-character value leaves the label 6 characters).
  The selectors and the `Active_preset` output (`modgui:monitoredOutputs`) say which preset is on.
* **What the Dwarf draws is the firmware's** (`mod-audio/mod-dwarf-controller`, `app/src/screen.c`, `mode_control.c`,
  `protocol.c`), so check it there before promising a look. A footer of a toggled or momentary control draws the name (at most 7
  characters in that source) and inverts when the value is on; an enumeration shares 7 characters between name and value. The
  firmware rewrites the LED of a **momentary** control on every press and release, over the plugin's colour and brightness
  (`lock_led_actions` guards only enumerations): the plugin's LED and label go out after the release, which is why the three
  buttons are momentary-off and why the plugin sends them again after each release (`labels_changed`). A name that was never
  changed ("Preset 3") is written "Prst 3" (`format_display_name`).
* **mod-host lives inside jackd** (the log says "Internal client mod-host"): restarting `mod-ui` does not reload the TTL it
  reads. After a change of ports (or of the defaults of a port) restart `jack2` and then `mod-ui` on the device, or the host
  never calls `addressed` for the new ports.
* **Display widgets** (`src/hmi/`, section below) are drawn through the optional aLabs `ahw:WidgetDraw` feature. Never make a plugin
  depend on it (stock firmware and the Duo do not have it): `widgets_init` leaves `draw` NULL and everything else does nothing.
* **Pop-ups (`popup_message`) only show on a device page that has an `Assign to notify` control addressed.** Keep
  the four notify ports and say so in the manual.
* Labels are plugin *properties* (`lv2:Parameter`, `atom:String`, `patch:writable`) read/written with
  `patch:Get`/`patch:Set` and saved through `state:interface`. After a state restore, send them back to the GUI
  (`labels_restored`). Sanitise them (`utils.c`: letters, digits, space, `+-:_.`, max 15) and keep the JavaScript
  `sanitizeLabel` in sync with it.
* Use `snprintf`, never `sprintf`, in new code. Check `malloc` results. Free what `*_cleanup` owns.

## TTL

* Every control port: `lv2:name`, `lv2:default/minimum/maximum`, `rdfs:comment` that says what it does and how to
  assign it, `pg:group` so the device's assignment screen is organised. Toggles are `lv2:toggled`;
  footswitch-like ones add `mod:preferMomentaryOnByDefault`.
* Keep `state:state` defaults (labels) and `patch:writable` equal to the parameters the code handles.
* Repetitive TTLs (dozens of dip-switch ports) are generated by a script, but only the result is committed: after
  generating, check the count with `get_plugin_info`, and keep the index comment at the top of the file.
* A new plugin starts at `lv2:minorVersion 0 ; lv2:microVersion 0` (LV2's "unstable"); raise it after device tests.
* **Raise `lv2:microVersion` whenever the modgui (HTML/CSS/JS) changes.** MOD UI serves those files with
  `Cache-Control: max-age=31536000` and its URL only changes with the plugin version
  (`?v=builder_micro_minor_release`): a changed file with the same version is not fetched again, so the device keeps
  running the old interface. While in development use odd numbers (1, 3, 5...), which LV2 reads as "nightly".
* Factory presets: one `pset:Preset` per scene setup, listing **every** port it sets and the `state:state` labels.
  Presets are listed in `manifest.ttl` with `rdfs:seeAlso <presets.ttl>`.

## modgui (HTML / CSS / JS)

* The plugin **stylesheet is global** (MOD UI appends it to the page) and it is run through Mustache: namespace every
  class (`.hept7`, `.h7s-*`), never write `{{`, and use zero-specificity resets (`:where()`) so component rules win.
* **One JavaScript function serves every instance** of the plugin: keep per-instance state in `event.data`, never in
  the closure. Events: `start` (ports and parameters), `change` (`symbol` or, for a `patch:Get` reply, `uri`),
  `patch_set`. Talk to the plugin with `funcs.set_port_value`, `patch_get`, `patch_set`. Show the effect of a click
  at once, and let the host's events have the last word: the host answers late, several clicks can be in flight, and
  MOD UI sends **nothing** when a port already holds the value (`setPortValue`: `port.value == value`), so a click can
  go unanswered. Never let a late answer to an old click undo a newer one, and never ignore a press because the page
  thinks it is already in that state. MOD UI's copy of a selector stays at 1 until the host answers, so a quick click
  back to it was dropped: Tetr4's buttons set `Preset_combobox`; Hept7's follow MOD UI's
  copy of each selector and send 0 then 1 when it is already 1, and take the answer from `Active_preset` (its `Preset_combobox` is a button the page never writes). The preview simulates this (host latency, dropped clicks): run
  `tools/modgui-preview/scenarios.html` after touching click handling. `event.icon` and `event.settings` are the two DOM roots.
* **MOD UI covers the whole icon with `.mod-drag-handle` (absolute, `z-index: 20`)** so the plugin can be dragged.
  Every clickable element of the icon (buttons, inputs) needs `position: relative; z-index: 30`, or it cannot be
  clicked on a real pedalboard. A script's `.click()` does not notice this; the preview shows a red bar for it.
  The settings window is opened by MOD UI with `z-index: 1000`, above the pedals.
* **MOD UI's Performance view (aLabs) shows the settings page too**, in `.js-performance-plugin-settings`, and breaks it twice.
  First, its stylesheet has `#performance header {position: fixed; height: 45px; ...}` and `#performance h1 {...}`: an ID beats any
  class, so **never use `<header>` or `<h1>` in a settings template** (a `div` with a class, `role="heading"` for the title).
  Second, it renders the template a *second* time (`isPerformanceView`), which no script ever receives (`event.settings` is the first
  copy): it stays the bare HTML. Each script has `adoptInPerformanceView`, which swaps the live page in while the view shows the
  plugin and gives it back to `<body>` (where MOD UI keeps its window, hidden) when the view closes or shows another plugin;
  `[data-performance]` hides the Close button. Test it by loading the device's MOD UI in a Chrome whose requests for
  `/effect/file/{iconTemplate,settingsTemplate,stylesheet,javascript}` are answered with the local files (CDP `Fetch`, as
  `text/plain`: the device serves them like that, and `application/javascript` makes jQuery run them).
  An overlay drawn over those controls (an announcement, a tooltip) then needs a higher one still, with `pointer-events: none`.
* The icon must not rely on MOD UI's film/knob widgets; draw it yourself. Outputs are laid out by MOD UI
  (`.mod-pedal-output`, 56 px per jack): with many outputs use a CSS grid with several columns.
* **A custom settings page** (`modgui:settingsTemplate`; the full guide is `docs/custom-settings-pages.md`, read it first): root absolutely positioned over the whole window, a
  `.js-close` button (the window closes with it), the assignment buttons as `[mod-role=input-control-address]` with
  `mod-port-symbol` (MOD UI adds the `addressed` class), and the saved-presets block copied from MOD UI's
  `html/resources/settings.html` (`.mod-presets` ...) so it keeps working. Rules `.mod-presets {width: 322px !important}`
  etc. need overriding.
* **Touch first**: targets of at least 44 px, `touch-action: manipulation`, no hover-only information,
  `prefers-reduced-motion` respected, layouts checked from 320 px to 1920 px with no horizontal scroll. Provide a
  fallback before any `color-mix()`.
* On the MOD Duo there are no plugin pop-ups: if the plugin changes something the musician should notice, the
  icon itself must show it.
* **The vendor is on the face of the plugin**: the header of every icon has the name of the plugin and, under it, the vendor
  in small type (`{{brand}}`, which is the `modgui:brand` of `modgui.ttl`: never write it by hand). The plugins other than Gossiper have no
  mark beside the name; Gossiper keeps its speech bubble.
* `screenshot.png` is about 500 px wide and `thumbnail.png` about 150 px, both transparent around the plugin. They stay **PNG**:
  MOD UI and MOD's tools read them (`modgui:screenshot`, `modgui:thumbnail`). The standard picture of a MOD pedal shows the
  **sockets** on both sides (the part of a jack box that sticks out: 6 to 16 px) and **no plug**: `shots.mjs` makes them with
  `plugs=0` and a crop that takes the sockets, the 2 px ring of the body and its shadow, so nothing is cut. A socket must stick out
  of the ring: a body with a `box-shadow` ring needs its output column moved out by the width of the ring (Tetr4 does).
* **Read-only information must not look like a control.** On the settings pages, what only shows a state (the pattern that plays,
  the active preset, the last control used, the preview of a pattern, the number tags of the rows) is flat: no fill, no shadow, no
  pill, no sunken box (raised means button, sunken means text field). A rule at its side in the colour of the state, the caption in small capitals and the value
  as plain text. Details in `docs/custom-settings-pages.md` (section 9).
* **The settings page has every control the icon has** (and more room to show it): the page of a plugin with a pedal face is the
  place to play it from a phone or a tablet, so nothing that is on the pedal may exist only there. Kill Switch: the top bar has Mono,
  Kill, the page arrows and Close; under it, on every page, **what plays** as a flat strip (`.kss-glance`: its name and the two rows of the
  steps); then the pages: the choice of pattern (every one at hand as a grid, or `< name >` arrows when the screen is too small for the grid:
  `data-choice` of `.kss`, decided by trying the layouts), the **steps as LEDs** (a recessed cell that lights, as Hept7's outputs do; white for
  the left side, red for the right one) which are also the editor, tempo and feel, and the saved presets. The LEDs of Custom change level
  with a tap; those of a built-in pattern open a dialog (`.kss-modal`, inside the page, so it has the theme of the plugin and survives the
  Performance view) that asks whether to define a Custom pattern from it: *yes* writes the length, the steps per beat and every step into the
  custom ports first and only then switches `pattern`. Length and steps per beat are with the LEDs and only show while Custom plays (`data-custom`;
  the layout measures with them shown, so the page does not jump).

## Display widgets

Plugins can draw on the display of a MOD Dwarf or Duo X where the device implements the aLabs HMI widgets extension
(https://github.com/sejerpz/alabs-hmi-widgets, header copied in `src/hmi/`; ISC, listed in `NOTICE`). Details and the preview tool: `tools/hmi-preview/README.md`.

* **The widget code stays out of the plugin's main file**: the plugin fills a view (what to show) and calls `*_widgets_run`; drawing, the list of what is on a display and the signature are in the widget file (`src/hmi/<plugin>-widgets.c`, `src/lv2/*/lv2-*-widgets.c`). Add the file to the plugin's `build-<name>` target.
* A widget is declared in the TTL (`ahw:widget`, with `ahw:minWidth/minHeight`, a natural size and `ahw:suggestedPorts`) and has a
  symbol that is kept in a pedalboard: **never rename it after a release**. The order of `WIDGET_SYMBOLS` is the plugin's own enum.
* A widget **never assumes a size**: it learns the area in `widget_shown`, and draws the whole area every time. Dwarf areas run from
  40 x 37 to 128 x 64 and footers of 10 px, the Duo X's from 63 x 8 to 128 x 64; a widget declares the smallest area it reads well in
  (at most 51 x 8 for a footer, 40 x 30 for a column) and adapts by size, not by device.
* The display is **1 bit**: no grey. Use the checker (`canvas_checker`) as a grey, the 5 x 7 font (6 px a character) where it fits
  and the 3 x 5 one where it does not; a text is cut, never wrapped. Read-only widgets show state: do not draw buttons.
* Drawing happens in `run()` through `widgets_run` (a *signature* of the state, redrawn when it changes, at most 20 Hz; the host
  merges and queues, it never blocks). `widget_shown` and `widget_hidden` come from another thread: use the seqlock slots of
  `hmi/widget-host.h`, do not keep the handle anywhere else.
* **A widget cannot learn from the host what MIDI, CC or CV assignments there are**: mod-host calls `addressed` for the device's own controls only. Gossiper's
  settings page watches the `addressed` class that MOD UI puts on its assign buttons and writes it into the `footswitch_in_use_N` ports (not on any page, not
  `notOnGUI`: that would take them out of the script's `set_port_value`). The widget shows the union of both.
* **Compare before and after** like any change to `run()` (`host.c` has `hash` for the audio and CV outputs), check with
  ASan/UBSan on every area size, and look at `tools/hmi-preview/preview.py gallery`. There is no device test here: say so.

## Documentation

* `Readme.md`: one section per plugin (what it is, features, links); update the build/`docker cp` lists.
* User manual: `plugins/resources/<name>.lv2/docs/manual.md` (+ `images/`), built into `modgui/manual.html` (section "HTML manuals")
  and, where a PDF is still wanted, into `modgui/manual.pdf` by `docs/build_manual.py` (needs `pip install markdown` and Chrome).
  `modgui:documentation` points to the HTML where it exists.
  `docs/` is not installed on the device, only `modgui/` is. Images come from the preview tool.

## Animations and alternative thumbnails

Every plugin has, in `plugins/resources/<name>.lv2/docs/` (not installed on the device), and all of them are shown in the
plugin's section of `Readme.md`:

* `images/<name>-demo`: the plugin on the pedalboard, used the way a musician would (a few taps on the footswitches
  and switches, a rename). `images/<name>-settings`: its settings page, going through every page with the arrows of
  the top bar. A plugin without a settings page of its own has no second animation. **Each one is a pair of files with the
  same name: `.webp` (animated, the one to show) and `.gif` (the fallback, for what cannot play WebP).** Show them together,
  never the GIF alone: in the Readme and on the site as `<picture><source srcset="x.webp" type="image/webp"><img src="x.gif"></picture>`,
  in `manual.md` as `![caption](images/x.webp)` (the manual builder finds the `.gif` next to it and writes the `<picture>`).
* `alternative-thumbnail.webp` (400x400, transparent outside the rounded square), drawn by `alternative-thumbnail.html`
  next to it: a **simpler, more abstract** picture than `thumbnail.png`, like the first image of a plugin in the Anagram
  marketplace: flat shapes in the colours of the plugin, **no text**, no real UI. It is for internal use (MOD UI does not
  read it); keep it in the Readme next to the title of the plugin.

The full guides, written for other developers, are [docs/making-animations.md](docs/making-animations.md) and
[docs/alternative-thumbnails.md](docs/alternative-thumbnails.md): keep them in step with the tools. In short (needs Node 22+, Google Chrome and `pip install pillow`; the preview starts by itself):

```bash
node tools/media/record.mjs all                    # every animation (.webp and .gif)
node tools/media/record.mjs tetr4-switch settings  # one: <plugin> <demo|settings>
node tools/media/thumbnails.mjs                    # every alternative-thumbnail.webp
node tools/media/shots.mjs all                     # the pictures of the manuals (docs/images/*.webp), modgui/screenshot.png and thumbnail.png
node tools/media/shots.mjs kill-switch screenshot.png   # one picture of one plugin
```

* `record.mjs` drives the preview (`tools/modgui-preview/frame.html`) through the Chrome DevTools, runs the script of
  `tools/media/scenarios.mjs`, captures frames while it runs and `build_gif.py` puts them together (identical frames are
  merged, one palette, endless loop). The input is shown as a **finger** (a circle that glides to what is tapped), not as
  a mouse pointer: the interfaces are touch first. The taps are real mouse events, so `:active` states are recorded.
* **Chrome must be cleaned up** (full rules: [docs/making-animations.md](docs/making-animations.md#9-cleaning-up-after-chrome)).
  Each picture/animation starts a headless Chrome with a 50-150 MB profile (`media-chrome-*` in the temporary folder);
  the tools remove it on a normal end only. Scratch scripts use `launch()` from `cdp.mjs` with `try/finally { page.close() }`
  (or `--user-data-dir="$(mktemp -d)"` and `rm -rf` it), never a Chrome started with `&`. After a failed or interrupted run, and
  at the end of any session that recorded a lot: `pgrep -fl 'media-chrome-'`, `pkill -f 'media-chrome-'`, then
  `rm -rf "${TMPDIR:-/tmp}"/media-chrome-* "${TMPDIR:-/tmp}"/frames-*`. Leaked profiles have filled the disk before.
* **Pictures in `docs/` are WebP, not PNG** (`docs/images/*.webp`, `alternative-thumbnail.webp`): they need transparency
  (JPEG cannot), and WebP keeps it at a fifth of the size of PNG. The tools write WebP directly; for a picture that comes from
  elsewhere, `python3 tools/media/to_webp.py FILE.png` converts it and rewrites the Markdown that mentions it (`--docs`: every
  PNG of the docs). The PNGs that MOD UI reads (`modgui/screenshot.png`, `thumbnail.png`) stay PNG. The animations are written by `record.mjs` as WebP and GIF; photos (JPEG) stay as they are.
* A scenario is short (10 to 20 s, under about 1 MB), starts and ends on the first preset, and shows the point of the plugin:
  what a press does, which output lights up, that a name is editable. The settings animation is recorded at 860 px wide
  (tablet size: it has pages); the pedalboard one is clipped to the plugin on the dark of a pedalboard. Plugins that only
  show what a controller did (Gossiper) are driven from outside with `send({type: 'change', symbol, value})`. Taps on
  hardware-assignment tiles mark them as `addressed`, which is what MOD UI does once its dialog is confirmed; do not
  show anything the real plugin does not do.
* **Re-record in the commit that changes the interface** (like the screenshot and the PDF of the manual), and look at
  the result: pull some frames out with Pillow before committing. When a selector of the interface changes, update the
  scenario.
* The alternative thumbnail is plain HTML/CSS using the colours of the plugin (copy the variables from its stylesheet);
  render it with `thumbnails.mjs` and look at it small, next to the others.

## HTML manuals

`modgui:documentation` points to `modgui/manual.html`, built from `docs/manual.md` (+ `docs/manual.json`, the cover and the
colours of the plugin) by `python3 tools/manual/build_manual_html.py <plugin>|all` (`pip install markdown pillow`). The
conventions of the Markdown (steps, callouts, video cards, recipes, FAQ, pins on a picture) and of the JSON are in
[docs/html-manuals.md](docs/html-manuals.md).

* **Put a picture in every step that happens in our interface** (not in MOD UI's own dialogs); a list item with a picture is
  indented by four spaces with a blank line around it, or the numbered list breaks.
* **`manual.md` is the source; never edit `manual.html`.** Rebuild in the commit that changes the text or the interface.
* The page must be **self-contained**: MOD UI serves it from `/effect/file/documentation`, where relative links do not
  work, so the builder embeds the CSS, the JS and every image. Keep images small; animations only where they tell something (the manual embeds both the WebP and its GIF fallback).
* Pictures must stay in proportion at every width (phone 390, tablet 820, desktop 1440; light and dark theme): look at
  the three widths before committing, especially the cover picture.
* **Pictures of the interface in a manual go stale when the interface changes** (a step that shows a button, a settings
  page). The ones of the steps are made by `tools/media/shots.mjs` from `tools/media/shot-specs.mjs` (state, ring, finger,
  crop): run it in the same commit, with the target highlighted and the finger on it, and search the
  manuals for what you changed. Pictures of MOD UI's own dialogs are not ours to redo.
* Facts in a manual come from the plugin (the TTL, the factory presets, the code), not from memory: check a recipe or a
  port against `presets.ttl` / the TTL.
* **The manual carries the version of the plugin it was written or updated for**: the `lv2:majorVersion.minorVersion.microVersion`
  of its TTL at that moment (`1.2.5`). Put it in `manual.json` (`"version": "v1.2.5"`, the cover) and in `manual.md` (the `Version` row of
  the Specifications table, or a last line *This manual is for version 1.2.5 of ...* when there is no table). Update both whenever you
  change the manual or rebuild it for a changed interface, in the same commit as the version bump; the builder warns when
  `manual.json` and the TTL differ.
* Raise `lv2:microVersion` when the manual changes (the modgui cache).

### The interactive manual

A manual can have one section in which **the real plugin runs inside the page**: a ```` ```interactive ```` block in `manual.md` plus `docs/interactive.json`
(the areas to tap and the guided tours). Full guide: [docs/interactive-manuals.md](docs/interactive-manuals.md); read it first. Done so far: Tetr4 Switch.

* **It is never a copy of the interface.** `tools/manual/interactive.py` embeds the bundle's own icon and settings templates, stylesheet and script
  (in a same-origin `srcdoc` iframe) and runs them against `tools/manual/interactive/host.js`, a stand-in for MOD UI (jQuery 1.9.1, the start event, `funcs`,
  the simulated host of `Preset_selector_N`, the settings window, MOD UI's assignment dialog drawn in a simplified way). Rebuild the manual and it follows the
  interface. Do not put plugin-specific code in `host.js`: add what the TTL gives (ports), as `tools/modgui-preview/frame.html` does.
* **A change to the interface can break it**: the selectors of `interactive.json` (`select`, `tap`, `ring`) are the classes of the modgui. When you rename a
  class or move a control, search `docs/interactive.json` and run every tour (Next to the end, then Back to the middle). Rebuild the manual in the same commit,
  with the version bump of the manual.
* **Tours must be replayable from the clean plugin**: Back and the step list reload the frame and redo what the steps before did, at once.
  Write the first step for the defaults of the TTL, and do not show what the plugin does not do.
* The area `text` and the tours say facts about the plugin: take them from the TTL, the factory presets and the code, like the rest of the manual.
* Drive the page with `launch()` of `tools/media/cdp.mjs` and close it (`try/finally`): see "Chrome must be cleaned up". The way in is
  `document.querySelector('.ix-frame').contentWindow.Host`.

## Documentation site

`tools/site/build_site.py` builds a static site (landing page, the user manual of each plugin, the guides of `docs/` and
the preview's README) that `.github/workflows/docs.yml` publishes on GitHub Pages: `python3 tools/site/build_site.py --out _site`
(`pip install markdown pillow`; Python 3.12+). It renders the manuals and the guides with `tools/manual/build_manual_html.py`,
so they look like the manuals of the device.

* **A new plugin** appears on the site by being added to `PLUGINS` in `tools/site/build_site.py` and having `docs/manual.md`,
  `docs/manual.json` (title, tagline, badges, colours, links), `docs/alternative-thumbnail.webp` and the two animations (.webp and .gif). **A new guide**
  is added to `GUIDES` (and `GUIDE_TAGLINES`) in the same file.
* A link from a guide to another guide becomes a link to its page; a link to any other file becomes a link to it on GitHub: write
  guides with relative links to files (`../CLAUDE.md`, `./making-animations.md`), never with absolute URLs of this repository.
* Fenced code blocks inside a list item are turned into code blocks after the item (Python-Markdown does not nest them);
  prefer a fence at the left margin.
* **The workflow publishes from the development branch for now** (a TODO at its top): when the branch is merged, replace it by
  the main one in `docs.yml`. Settings needed once: *Pages > Source: GitHub Actions*, and the branch allowed in
  *Environments > github-pages > Deployment branches*.

## Git

* Never commit build output or editor/Python leftovers: `*.so`, `__pycache__/`, `*.pyc`, `.DS_Store`, `out/`,
  `binaries/` (all in `.gitignore`). Run `git status` before every commit and add files deliberately.
* Commit messages: `<plugin or area>: what changed` in the subject, the why in the body. Small, reviewable commits;
  generated files (PDF, PNG) go in the commit of the change that produced them.
