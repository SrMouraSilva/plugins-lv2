# modgui preview

Look at (and click through) the interface of the plugins of this repository in your browser, without installing
them on a device. It works with every bundle in `plugins/resources/*.lv2` that has a `modgui`.

```bash
# MOD UI is only needed for its stylesheets and scripts: clone it next to this repository
git clone https://github.com/moddevices/mod-ui ../mod-ui

python3 tools/modgui-preview/serve.py        # or: --mod-ui PATH/mod-ui/html, or MOD_UI_HTML=...
```

Open <http://localhost:8000/> (`?plugin=gossiper` picks the plugin). The files of the plugin are read from disk on
every request, so edit `plugins/resources/<plugin>.lv2/modgui/*` and reload. Only the Python standard library is used.

* **Plugin**: any bundle. Ports, parameters, the default state and the factory presets are read from its TTL files
  (`<plugin>.ttl`, `presets.ttl`, `modgui.ttl`), so a new plugin needs no change here
* **View**: the plugin on the pedalboard, or its settings page, if it has its own (`modgui:settingsTemplate`).
  Plugins without one use the generic settings window of MOD UI, which is not previewed
* **Screen**: the page is rendered inside a frame of that width, so a responsive layout can be checked
* **Preset**: starts the interface with the values of a factory preset
* **Ports**: change a control port as the host would (a footswitch, a knob, MIDI...), to see how the interface
  reacts, for instance the display of Hept7 Switch announcing a change of preset (filter `selector`, tick one)

* **Host answers after**: for plugins with `Preset_selector_N` ports the frame also simulates MOD UI and the plugin: it
  sends nothing when a port already has that value (as `setPortValue` does), and answers after the chosen delay by
  turning the previous selector off, the new one on and moving `Preset_combobox`, leaving out, as mod-host does, the
  port the change came through (the host already holds that value). It reproduces races that a script's instant
  `.click()` never shows. `scenarios.html` runs eight of them (quick clicks, a dropped click, a change from
  outside...) at several delays and says which pass; `scenarios.html?run=1&latency=400` runs them at once and puts
  the number of failures in the page title, for a headless browser
* **Clickable check**: after loading, the frame tests (`elementFromPoint`) that every button/input of the visible view
  can actually be reached, and shows a red bar listing the ones that something covers. MOD UI puts a drag handle
  (`z-index: 20`) over the whole icon, so a control without `position: relative; z-index: 30` cannot be clicked on a
  real pedalboard, even though a script's `.click()` works. The frame also gives the plugin icon and the settings
  window the same `z-index` MOD UI gives them (30 and 1000)

It loads MOD UI's own CSS, jQuery and Mustache and renders the templates the way `modgui.js` does, but there is no
plugin behind it: the ports only hold the values they are given, and what the interface asks the plugin to do is
only logged (`__log` in the frame's console). Apart from the selector simulation above, the plugin does not react
to its own controls. It does not replace a test on a real pedalboard (stacking of the
settings window, the position of the jacks, the device HMI, ...). The C side is not covered here.

`ttl.py` is a small Turtle reader for the subset used by these bundles. If a TTL uses something it does not
understand, the page says so.
