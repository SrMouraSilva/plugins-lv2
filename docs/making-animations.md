# Making animations of a plugin (animated WebP, with a GIF as the fallback)

A short animation of the plugin being used is the best first thing a visitor sees in a Readme or in a manual. This
guide explains how this repository records them (`tools/media/`), so that **you can do the same for your own plugin**,
and what makes a good one. The animations are not screen recordings: a script drives the interface in a browser, so they
are **reproducible**, clean (no desktop, no cursor jitter) and can be re-recorded in one command whenever the interface
changes.

> [!IMPORTANT]
> **Show the animated WebP; keep the GIF only as the fallback.** Every animation is written twice, with the same
> name: `name.webp` (the one to show: full colour, 24-bit, every current browser plays it) and `name.gif` (what an old
> browser, a viewer or an e-mail client shows when it cannot play WebP: 256 colours at most, so it is a little worse).
> They go together in a `<picture>` (section 7): the browser takes the first `<source>` it understands, so nobody
> downloads both. Do not link the GIF alone.

Every plugin of this repository has two animations, each as a pair:

| Animation | Shows | Example |
|---|---|---|
| `<plugin>-demo.webp` + `.gif` | The plugin on the pedalboard, used the way a musician would use it | `plugins/resources/tetr4-switch.lv2/docs/images/tetr4-switch-demo.webp` |
| `<plugin>-settings.webp` + `.gif` | Its custom settings page, going through every page | `.../tetr4-switch-settings.webp` |

Contents

1. [How it works](#1-how-it-works)
2. [What you need](#2-what-you-need)
3. [Use it in your repository](#3-use-it-in-your-repository)
4. [Writing a scenario](#4-writing-a-scenario)
5. [What makes a good animation](#5-what-makes-a-good-animation)
6. [Checking the result](#6-checking-the-result)
7. [Putting it in the Readme and the manual](#7-putting-it-in-the-readme-and-the-manual)
8. [Troubleshooting](#8-troubleshooting)
9. [Cleaning up after Chrome](#9-cleaning-up-after-chrome)

---

## 1. How it works

```
                                                                        ┌──►  <plugin>-<kind>.webp  (animated, shown)
scenarios.mjs  ──►  record.mjs  ──►  frames (PNG) + durations  ──►  build_gif.py ─┤
 (what to do)      drives Chrome         captured while the           Pillow       └──►  <plugin>-<kind>.gif   (fallback)
                   through DevTools      script runs
                        ▲
                        └── the modgui preview (tools/modgui-preview): renders your icon and settings templates
                            with MOD UI's own CSS and jQuery, and answers like a host would
```

* **The preview** (`python3 tools/modgui-preview/serve.py`, see its README) loads the modgui of any bundle. It starts
  by itself when you record. Nothing runs on a device, so the plugin's own sound is not part of the animation: only its
  interface.
* **`record.mjs`** opens the preview in headless Google Chrome through the DevTools protocol (`cdp.mjs`, no
  dependencies), runs a *scenario*, and captures frames in the background for as long as the scenario runs. A frame
  lasts until the next one was taken, so what you see is the real timing of the animations.
* **The finger.** Input is shown as a translucent circle that glides to what is tapped, shrinks while it is pressed and
  fades when released: modgui interfaces are touch first, so a mouse pointer would be the wrong picture. The taps are
  real mouse events (`Input.dispatchMouseEvent`), so `:hover` and `:active` states are recorded too.
* **`build_gif.py`** merges identical frames in a row (a hold costs one frame) and writes **both files from the same
  frames**, so they show the same thing for the same time, as an endless loop:
  * the **WebP** keeps every colour (lossy, `WEBP_QUALITY`);
  * the **GIF** has 256 colours at most: it builds **one palette** from frames spread over the whole animation (no
    flicker, and a colour that shows up in one scene only, such as a preset colour, is not lost).

## 2. What you need

* Node 22 or newer (the global `WebSocket` is used) and Google Chrome (`CHROME_BIN` if it is not in the usual place).
* Python 3 with Pillow: `pip install pillow` (the wheels come with WebP, including animated; `python3 -c "from PIL import features; print(features.check('webp'))"` says `True`).
* The preview needs a checkout of [mod-ui](https://github.com/moddevices/mod-ui) next to your repository (see
  `tools/modgui-preview/README.md`). Only its CSS and JavaScript are used.

## 3. Use it in your repository

Copy two folders: `tools/modgui-preview/` and `tools/media/`. Then change what is specific to this repository:

| Where | What |
|---|---|
| `tools/media/record.mjs` | `ROOT` and the output path (`plugins/resources/<plugin>.lv2/docs/images/`), if your bundles live elsewhere |
| `tools/media/scenarios.mjs` | the scenarios: one object per plugin (see below) |

```bash
node tools/media/record.mjs all                    # every animation (.webp and .gif) of every plugin in scenarios.mjs
node tools/media/record.mjs my-plugin demo         # one animation: <plugin> <kind>
KEEP_FRAMES=/tmp/frames node tools/media/record.mjs my-plugin demo   # also keep the frames, to try other encoder settings
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
  panel, inputs on the left and outputs on the right; add `plugs=0` too for the picture of the store, which shows the
  sockets and not the plugs that MOD UI draws on the outputs). **`view=settings`** shows the settings page at the size of the window: use a tablet-sized window
  (860 px wide) so that the page has several pages and the arrows are part of the story.
* **Pages of a settings page**: tap the arrow of the top bar, wait for the scroll to end (about 900 ms), then tap
  something on the new page. `visitPages()` in `scenarios.mjs` does this for any plugin and comes back to the first page; its `onPage(page)` hook runs something on a page once it is shown (Tetr4 Switch touches the fields of its MIDI page).
* **A plugin that only shows what happened** (a meter, a status display) cannot be tapped: drive it from outside with
  `ctx.eval("send({type:'change', symbol, value})")`, as a MIDI controller or the host would.
* **Latency**: `latency=150` in the query makes the simulated host answer 150 ms after a click, like the real one. Keep
  it: it shows what the user really sees.
* **Do not show anything the real plugin does not do.** A tap on a hardware-assignment tile opens a dialog in MOD UI that
  the preview does not have; the scenarios mark the tile as `addressed` afterwards, which is what MOD UI does once the
  dialog is confirmed. That is the only kind of shortcut that is acceptable.

## 5. What makes a good animation

* **Tell one story in 10 to 20 seconds**: what a press does, which output lights up, that a name is editable. Start and
  end in the same state (the first preset), so that the loop is seamless.
* **Hold on the result.** After a tap, wait long enough to read what changed (600 to 900 ms; 2 s when something is
  announced, such as a toast). An animation that never rests is tiring.
* **Show the plugin only.** Clip to the plugin with a margin, **jacks included** when they stick out of its body (the
  `around(selector, margin, jacks)` of `scenarios.mjs`); the settings animation fills the window.
* **Size**: under about 1 MB each. The recorder merges identical frames; a calmer scenario is smaller. If one is too
  big, shorten it before lowering the quality. The WebP is encoded lossy at quality 80 with `minimize_size`
  (`build_gif.py`): in this repository it is **a third to a half smaller than the GIF** of the same frames, and keeps every
  colour (Hept7 Switch demo: 208 KB against 347 KB; Gossiper settings: 530 KB against 860 KB). Lossless would be four to
  five times larger than the GIF: do not use it.
* **Frame durations below 40 ms** are raised to 40 ms by `build_gif.py` (browsers show anything shorter than 20 ms as
  100 ms).
* **No text baked into the picture.** Captions belong in the Readme or in the manual, where they can be translated.

## 6. Checking the result

Never commit an animation you have not looked at. Pull some frames out of **both** files with Pillow and look at them:

```python
from PIL import Image
for name in ('my-plugin-demo.webp', 'my-plugin-demo.gif'):
    animation = Image.open(name)
    print(name, animation.n_frames, animation.size)       # the same number of frames, the same size
    for i in (0, animation.n_frames // 3, 2 * animation.n_frames // 3):
        animation.seek(i); animation.convert('RGB').save(f'{name}-{i}.png')
```

Look for: a colour that turned grey **in the GIF** (the palette was built from too few frames: more variety in the
scenario, or raise `COLORS`/`SAMPLE_FRAMES` in `build_gif.py`; the WebP does not have this problem), a control cut by the
clip (the jacks!), the finger on the wrong element, a name field left in focus at the end. The two files must
tell the same story: if they differ in the number of frames, one of them was not rebuilt.

## 7. Putting it in the Readme and the manual

The animation is a `<picture>`: the WebP first, the GIF as the image it falls back to.

* **Readme** (GitHub shows it; Markdown has no syntax for this, so it is HTML), under the title of the plugin:

  ```html
  <picture>
    <source srcset="path/to/name-demo.webp" type="image/webp">
    <img src="path/to/name-demo.gif" alt="Name on a pedalboard">
  </picture>
  ```

* **HTML manual** (see [html-manuals.md](./html-manuals.md)): in `docs/manual.md` write the **WebP only**, as a normal
  figure, `![Name in use](images/name-demo.webp)`. When `images/name-demo.gif` sits next to it the builder embeds both
  and writes the `<picture>` itself. Use the demo near the top and the settings animation in the settings section; they
  are the largest files of the manual.
* **A web page of your own**: the same `<picture>`, with `loading="lazy"` on the `<img>` (the documentation site does).
* **Do not use** an animated WebP in `modgui/` or in anything MOD UI reads (`screenshot.png`, `thumbnail.png` stay PNG).
* **Re-record in the commit that changes the interface**, like the screenshot and the manual.

## 8. Troubleshooting

| Symptom | Cause |
|---|---|
| `ModuleNotFoundError: PIL` | Pillow is not installed for the `python3` that `record.mjs` runs; do not run it with `python3 -I` (that hides user-installed packages) |
| Chrome does not start | set `CHROME_BIN` to the executable |
| The animation is cut at the bottom or on the right | the `viewport` is smaller than the `clip`, or the clip leaves out the jacks that stick out of the plugin |
| The WebP shows no animation (one frame) | an old Pillow without animated WebP: update it (`pip install -U pillow`); the GIF is still written |
| The tap does nothing | the element is covered (the preview shows a red bar for this), or the selector matches an earlier, hidden element: `tap` uses the first match |
| A colour is wrong in the GIF | see section 6 (the palette); the WebP keeps every colour |
| `--virtual-time-budget` screenshots look different from the recording | do not use it for pages that measure themselves (layout scripts run on real time); `record.mjs` runs in real time |
| The disk fills up after a few runs | a run that failed or was interrupted left its Chrome and its `media-chrome-*` profile: see section 9 |
| The page shows an old interface | the preview reads the files on every request; reload, and check that you edited the bundle the scenario loads |

## 9. Cleaning up after Chrome

Every picture and every animation starts its **own headless Chrome** (`launch()` in `cdp.mjs`) with a throw-away profile,
`media-chrome-*` in the temporary folder. A profile is **50 to 150 MB**, and a Chrome that is never closed keeps running
besides: a few forgotten runs are enough to fill a small disk. The tools clean up after themselves, **as long as they
end normally**:

* `page.close()` closes the connection, stops Chrome and deletes the profile. `record.mjs`, `shots.mjs` and
  `thumbnails.mjs` call it when a picture or an animation is done.
* `record.mjs` also deletes its frames (`frames-<plugin>-<kind>-*`) when the animation is built, and the three scripts
  stop the preview server they started.

What is **not** covered: a run that dies half-way (a scenario that throws, a selector that matches nothing, Ctrl-C, a
terminal that is closed). The page is not closed in a `finally`, so that Chrome stays alive and its profile (and the
frames) stay on disk. **After any run that failed or was interrupted, look:**

```bash
pgrep -fl 'media-chrome-'                                   # Chrome started by these tools and still alive
pkill -f 'media-chrome-'                                    # stop it (your own Chrome has no media-chrome- in its command line)
ls -d "${TMPDIR:-/tmp}"/media-chrome-* "${TMPDIR:-/tmp}"/frames-* 2>/dev/null
rm -rf "${TMPDIR:-/tmp}"/media-chrome-* "${TMPDIR:-/tmp}"/frames-*     # only after the pkill: Chrome writes to its profile until it ends
df -h /                                                     # what is free now
```

Rules for the scripts you write yourself (to measure a layout, to try a picture, to check a size):

* **Use `launch()` from `cdp.mjs`, not a Chrome of your own**, and put the work in `try { ... } finally { await page.close(); }`:
  the `finally` is what makes the cleanup survive an error.
* **If you must start Chrome by hand**, always pass `--user-data-dir="$(mktemp -d)"` and delete that folder afterwards.
  Without the flag Chrome uses (or hijacks) your everyday profile; with a fixed name it keeps a profile per run
  (a forgotten `cdp-profile-<number>` folder per run is how 36 profiles, 780 MB, piled up in the past).
* **One Chrome at a time, in the foreground.** Do not start a Chrome with `&` or `nohup` inside a script and walk away:
  nobody is left to close it. When a long run must go to the background, run the whole loop there, in the foreground
  of that background job, and check afterwards that nothing is left (`pgrep -fl 'media-chrome-'`).
* **Do not leave scratch files** (frames kept with `KEEP_FRAMES`, comparison pictures, throw-away scripts) in the
  temporary folder: delete them when the work is done, or put them in a folder you remove at the end.
* **Look before you finish** a work session that recorded or shot a lot: run the commands above once more. Nothing
  should be left; if something is, the run that left it is the one to fix.
