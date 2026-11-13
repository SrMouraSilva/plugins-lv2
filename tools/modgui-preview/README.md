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

* **Clickable check**: after loading, the frame tests (`elementFromPoint`) that every button/input of the visible view
  can actually be reached, and shows a red bar listing the ones that something covers. MOD UI puts a drag handle
  (`z-index: 20`) over the whole icon, so a control without `position: relative; z-index: 30` cannot be clicked on a
  real pedalboard, even though a script's `.click()` works. The frame also gives the plugin icon and the settings
  window the same `z-index` MOD UI gives them (30 and 1000)

It loads MOD UI's own CSS, jQuery and Mustache and renders the templates the way `modgui.js` does, but there is no
plugin behind it: the ports only hold the values they are given, and what the interface asks the plugin to do is
only logged (`__log` in the frame's console). So the plugin does not react to its own controls (for example, a
selector does not turn the other selectors off). It does not replace a test on a real pedalboard (stacking of the
settings window, the position of the jacks, the device HMI, ...). The C side is not covered here.

`ttl.py` is a small Turtle reader for the subset used by these bundles. If a TTL uses something it does not
understand, the page says so.
