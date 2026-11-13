# modgui preview

Look at (and click through) the Hept7 Switch interface in your browser, without installing the plugin on a device.

```bash
# MOD UI is only needed for its stylesheets and scripts: clone it next to this repository
git clone https://github.com/moddevices/mod-ui ../mod-ui

python3 tools/modgui-preview/serve.py        # or: --mod-ui PATH/mod-ui/html, or MOD_UI_HTML=...
```

Open <http://localhost:8000/>. The files of the plugin are read from disk on every request, so edit
`plugins/resources/hept7-switch.lv2/modgui/*` and reload.

* **View**: the plugin on the pedalboard, or its full-screen settings page (the page you get with the gear icon)
* **Screen**: the page is rendered inside a frame of that width, so the responsive layout can be checked
* **The host selects preset**: simulates what happens when a footswitch (or the knob) changes the preset, to see the display announce it

It loads MOD UI's own CSS, jQuery and Mustache and renders the templates the way `modgui.js` does, but there is no
plugin behind it: what the interface asks the plugin to do is only logged (`__log` in the frame's console).
It does not replace a test on a real pedalboard (stacking of the settings window, the position of the jacks, the
device HMI, ...). The C side is not covered here.
