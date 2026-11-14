# Making GIFs of a plugin

A short GIF of the plugin being used is the best first thing a visitor sees in a Readme or in a manual. This guide
explains how this repository records them (`tools/media/`), so that **you can do the same for your own plugin**, and
what makes a good one. The GIFs are not screen recordings: a script drives the interface in a browser, so they are
**reproducible**, clean (no desktop, no cursor jitter) and can be re-recorded in one command whenever the interface
changes.

Every plugin of this repository has two:

| GIF | Shows | Example |
|---|---|---|
| `<plugin>-demo.gif` | The plugin on the pedalboard, used the way a musician would use it | `plugins/resources/tetr4-switch.lv2/docs/images/tetr4-switch-demo.gif` |
| `<plugin>-settings.gif` | Its custom settings page, going through every page | `.../tetr4-switch-settings.gif` |

Contents

1. [How it works](#1-how-it-works)
2. [What you need](#2-what-you-need)
3. [Use it in your repository](#3-use-it-in-your-repository)
4. [Writing a scenario](#4-writing-a-scenario)
5. [What makes a good GIF](#5-what-makes-a-good-gif)
6. [Checking the result](#6-checking-the-result)
7. [Putting it in the Readme and the manual](#7-putting-it-in-the-readme-and-the-manual)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. How it works

```
scenarios.mjs  ──►  record.mjs  ──►  frames (PNG) + durations  ──►  build_gif.py  ──►  <plugin>-<kind>.gif
 (what to do)      drives Chrome         captured while the                Pillow
                   through DevTools      script runs
                        ▲
                        └── the modgui preview (tools/modgui-preview): renders your icon and settings templates
                            with MOD UI's own CSS and jQuery, and answers like a host would
```

* **The preview** (`python3 tools/modgui-preview/serve.py`, see its README) loads the modgui of any bundle. It starts
  by itself when you record. Nothing runs on a device, so the plugin's own sound is not part of the GIF: only its
  interface.
* **`record.mjs`** opens the preview in headless Google Chrome through the DevTools protocol (`cdp.mjs`, no
  dependencies), runs a *scenario*, and captures frames in the background for as long as the scenario runs. A frame
  lasts until the next one was taken, so what you see is the real timing of the animations.
* **The finger.** Input is shown as a translucent circle that glides to what is tapped, shrinks while it is pressed and
  fades when released: modgui interfaces are touch first, so a mouse pointer would be the wrong picture. The taps are
  real mouse events (`Input.dispatchMouseEvent`), so `:hover` and `:active` states are recorded too.
* **`build_gif.py`** merges identical frames in a row (a hold costs one frame), builds **one palette** from frames spread
  over the whole GIF (no flicker, and a colour that shows up in one scene only, such as a preset colour, is not lost) and
  saves an endless loop.

## 2. What you need

* Node 22 or newer (the global `WebSocket` is used) and Google Chrome (`CHROME_BIN` if it is not in the usual place).
* Python 3 with Pillow: `pip install pillow`.
* The preview needs a checkout of [mod-ui](https://github.com/moddevices/mod-ui) next to your repository (see
  `tools/modgui-preview/README.md`). Only its CSS and JavaScript are used.

## 3. Use it in your repository

Copy two folders: `tools/modgui-preview/` and `tools/media/`. Then change what is specific to this repository:

| Where | What |
|---|---|
| `tools/media/record.mjs` | `ROOT` and the output path (`plugins/resources/<plugin>.lv2/docs/images/`), if your bundles live elsewhere |
| `tools/media/scenarios.mjs` | the scenarios: one object per plugin (see below) |

```bash
node tools/media/record.mjs all                    # every GIF of every plugin in scenarios.mjs
node tools/media/record.mjs my-plugin demo         # one GIF: <plugin> <kind>
PREVIEW_PORT=8123 node tools/media/record.mjs all  # when 8000 is taken
```

A plugin needs nothing special to be recordable, as long as the preview can show it. If you already use the preview to
develop the interface, you are done.

## 4. Writing a scenario

A scenario is a plain object in `scenarios.mjs`:

```js
'my-plugin': {
  demo: {
    viewport: { width: 900, height: 640 },       // the browser window: must be bigger than what you clip
    query: 'view=shot&latency=150&jacks=1',      // the preview frame: view=shot is the icon alone, view=settings the settings page
    background: '#14161d',                       // behind the plugin (the dark of a pedalboard)
    clip: `(() => { const r = document.querySelector('.my-plugin').getBoundingClientRect();
                    return { x: r.left - 14, y: r.top - 14, width: r.width + 28, height: r.height + 28 }; })()`,
    async run(ctx) {
      await ctx.sleep(600);                      // let the first frame settle
      await ctx.tap('.mp-pad[data-n="2"]');      // the finger glides to it and taps
      await ctx.sleep(700);                      // let the animation play: this is what the viewer sees
      await ctx.type('.mp-name', 'Lead');        // taps the field, selects its text, types letter by letter
      await ctx.eval(`send({ type: 'change', symbol: 'Level', value: 0.5 })`);   // the "host" moves a control
    },
    hold: 900,                                   // how long the last frame stays, in ms
  },
}
```

`ctx` has: `tap(selector, holdMs?)`, `type(selector, text)`, `eval(expression)` (runs in the preview page, where `send()`
delivers an event to your plugin's JavaScript and `portValues` holds the ports), `sleep(ms)` and `page` (the raw DevTools
page, for anything else).

Things that work well:

* **`view=shot`** shows the icon alone on a transparent page (add `jacks=1` when the jacks are part of the plugin's
  panel). **`view=settings`** shows the settings page at the size of the window: use a tablet-sized window
  (860 px wide) so that the page has several pages and the arrows are part of the story.
* **Pages of a settings page**: tap the arrow of the top bar, wait for the scroll to end (about 900 ms), then tap
  something on the new page. `visitPages()` in `scenarios.mjs` does this for any plugin and comes back to the first page.
* **A plugin that only shows what happened** (a meter, a status display) cannot be tapped: drive it from outside with
  `ctx.eval("send({type:'change', symbol, value})")`, as a MIDI controller or the host would.
* **Latency**: `latency=150` in the query makes the simulated host answer 150 ms after a click, like the real one. Keep
  it: it shows what the user really sees.
* **Do not show anything the real plugin does not do.** A tap on a hardware-assignment tile opens a dialog in MOD UI that
  the preview does not have; the scenarios mark the tile as `addressed` afterwards, which is what MOD UI does once the
  dialog is confirmed. That is the only kind of shortcut that is acceptable.

## 5. What makes a good GIF

* **Tell one story in 10 to 20 seconds**: what a press does, which output lights up, that a name is editable. Start and
  end in the same state (the first preset), so that the loop is seamless.
* **Hold on the result.** After a tap, wait long enough to read what changed (600 to 900 ms; 2 s when something is
  announced, such as a toast). A GIF that never rests is tiring.
* **Show the plugin only.** Clip to the plugin with a margin; the settings GIF fills the window.
* **Size**: under about 1 MB. The recorder merges identical frames; a calmer scenario is smaller. If a GIF is too big,
  shorten it before reducing the colours.
* **Frame durations below 40 ms** are raised to 40 ms by `build_gif.py` (browsers show anything shorter than 20 ms as
  100 ms).
* **No text baked into the picture.** Captions belong in the Readme or in the manual, where they can be translated.

## 6. Checking the result

Never commit a GIF you have not looked at. Pull some frames out with Pillow and look at them:

```python
from PIL import Image
gif = Image.open('my-plugin-demo.gif')
print(gif.n_frames, gif.size)
for i in (0, gif.n_frames // 3, 2 * gif.n_frames // 3):
    gif.seek(i); gif.convert('RGB').save(f'frame{i}.png')
```

Look for: a colour that turned grey (the palette was built from too few frames: more variety in the scenario, or raise
`COLORS`/`SAMPLE_FRAMES` in `build_gif.py`), a control cut by the clip, the finger on the wrong element, a name field
left in focus at the end.

## 7. Putting it in the Readme and the manual

* **Readme**: a normal image, under the title of the plugin: `![Name on a pedalboard](path/to/name-demo.gif)`.
* **HTML manual** (see [html-manuals.md](./html-manuals.md)): the same line in `docs/manual.md`. The builder embeds the
  GIF in the page. Use the demo GIF near the top and the settings GIF in the settings section; they are the largest files
  of the manual.
* **Re-record in the commit that changes the interface**, like the screenshot and the manual.

## 8. Troubleshooting

| Symptom | Cause |
|---|---|
| `ModuleNotFoundError: PIL` | Pillow is not installed for the `python3` that `record.mjs` runs; do not run it with `python3 -I` (that hides user-installed packages) |
| Chrome does not start | set `CHROME_BIN` to the executable |
| The GIF is cut at the bottom | the `viewport` is smaller than the `clip` |
| The tap does nothing | the element is covered (the preview shows a red bar for this), or the selector matches an earlier, hidden element: `tap` uses the first match |
| A colour is wrong | see section 6 (the palette) |
| `--virtual-time-budget` screenshots look different from the recording | do not use it for pages that measure themselves (layout scripts run on real time); `record.mjs` runs in real time |
| The page shows an old interface | the preview reads the files on every request; reload, and check that you edited the bundle the scenario loads |
