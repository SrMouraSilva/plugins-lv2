# CLAUDE.md

LV2 utility plugins for MOD devices (MOD Dwarf first; Duo and Duo X must keep working). Plugins: **Gossiper**,
**Tetr4 Switch** and **Hept7 Switch**. Code, comments, commits and docs are in English.

## Layout

```
plugins/src/                 C sources (shared code in lv2/, model/, utils/)
plugins/resources/<name>.lv2 the bundle: <name>.ttl, manifest.ttl, modgui.ttl, presets.ttl, modgui/, docs/
plugins/Makefile             build-<plugin> targets; srmourasilva-plugins.mk is the mod-plugin-builder package
tools/modgui-preview/        browser preview of any plugin's modgui (no device needed)
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
* Look at the interface with `python3 tools/modgui-preview/serve.py` (see its README). It does not replace a test
  on a real pedalboard or device: say so when you could not test there.

## Adding a plugin (checklist)

1. Bundle in `plugins/resources/<name>.lv2` (TTLs, `modgui/`). Port symbols and parameter URIs are saved inside
   users' pedalboards: **never rename or renumber them after a release**; add new ones at the end.
2. `plugins/Makefile`: `build-<name>`, and add it to `build`, `clean`, `install`, `install-user`, `uninstall-user`.
3. `plugins/srmourasilva-plugins.mk`: add `<name>.lv2` to `SRMOURASILVA_PLUGINS_BUNDLES` and **bump
   `SRMOURASILVA_PLUGINS_VERSION`** (otherwise the builder does not rebuild).
4. `.github/workflows/release.yml`: add it to `PLUGINS`.
5. `Readme.md` section, `NOTICE` title, a user manual (below), screenshot and thumbnail.

## C code and LV2

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
* **Hept7's `Current preset` is a cycle** (`Preset_N_in_cycle`). The device keeps its own position for an enumeration on
  a footswitch, the B+C pair or a knob, and nobody updates it when the plugin changes the preset (`set_value` only
  changes the text). So a move of one option is a step counted from the active preset, and the plugin only ever writes
  the device's last value or the option opposite to it into the port (`Controller_may_show_in_combobox`): then a step
  is never ambiguous and never lost, even after MOD UI gives the device the port's value again. The selectors and the
  `Active_preset` output (`modgui:monitoredOutputs`) say which preset is on, not the combobox. Run the cycle test
  (a host whose "device" keeps its own index) after touching this.
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
  back to it was dropped: Tetr4's buttons set `Preset_combobox`; Hept7's (whose combobox is a cycle) follow MOD UI's
  copy of each selector and send 0 then 1 when it is already 1, and take the answer from `Active_preset`. The preview simulates this (host latency, dropped clicks): run
  `tools/modgui-preview/scenarios.html` after touching click handling. `event.icon` and `event.settings` are the two DOM roots.
* **MOD UI covers the whole icon with `.mod-drag-handle` (absolute, `z-index: 20`)** so the plugin can be dragged.
  Every clickable element of the icon (buttons, inputs) needs `position: relative; z-index: 30`, or it cannot be
  clicked on a real pedalboard. A script's `.click()` does not notice this; the preview shows a red bar for it.
  The settings window is opened by MOD UI with `z-index: 1000`, above the pedals.
  An overlay drawn over those controls (an announcement, a tooltip) then needs a higher one still, with `pointer-events: none`.
* The icon must not rely on MOD UI's film/knob widgets; draw it yourself. Outputs are laid out by MOD UI
  (`.mod-pedal-output`, 56 px per jack): with many outputs use a CSS grid with several columns.
* **A custom settings page** (`modgui:settingsTemplate`): root absolutely positioned over the whole window, a
  `.js-close` button (the window closes with it), the assignment buttons as `[mod-role=input-control-address]` with
  `mod-port-symbol` (MOD UI adds the `addressed` class), and the saved-presets block copied from MOD UI's
  `html/resources/settings.html` (`.mod-presets` ...) so it keeps working. Rules `.mod-presets {width: 322px !important}`
  etc. need overriding.
* **Touch first**: targets of at least 44 px, `touch-action: manipulation`, no hover-only information,
  `prefers-reduced-motion` respected, layouts checked from 320 px to 1920 px with no horizontal scroll. Provide a
  fallback before any `color-mix()`.
* On the MOD Duo there are no plugin pop-ups: if the plugin changes something the musician should notice, the
  icon itself must show it.
* `screenshot.png` is about 500 px wide and `thumbnail.png` about 150 px, both transparent around the plugin.

## Documentation

* `Readme.md`: one section per plugin (what it is, features, links); update the build/`docker cp` lists.
* User manual: `plugins/resources/<name>.lv2/docs/manual.md` (+ `images/`), built into `modgui/manual.pdf` by
  `docs/build_manual.py` (needs `pip install markdown` and Chrome) and referenced with `modgui:documentation`.
  `docs/` is not installed on the device, only `modgui/` is. Images come from the preview tool.

## Git

* Never commit build output or editor/Python leftovers: `*.so`, `__pycache__/`, `*.pyc`, `.DS_Store`, `out/`,
  `binaries/` (all in `.gitignore`). Run `git status` before every commit and add files deliberately.
* Commit messages: `<plugin or area>: what changed` in the subject, the why in the body. Small, reviewable commits;
  generated files (PDF, PNG) go in the commit of the change that produced them.
