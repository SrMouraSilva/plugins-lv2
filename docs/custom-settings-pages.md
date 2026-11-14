# Building a custom settings page for a MOD plugin

A MOD plugin can replace the generic settings window of MOD UI with a page of its own
(`modgui:settingsTemplate`). This guide collects what we learned building the one of **Hept7 Switch**
(`plugins/resources/hept7-switch.lv2/modgui/`), the first one in this repository. Read it next to the
[CLAUDE.md](../CLAUDE.md) rules for modgui, which it expands; the code samples are shortened copies of that plugin's
files, which are the reference when a detail is missing here.

Do it when the generic window does not fit the plugin: many related controls that need a layout of their own (a matrix
of presets and outputs), or text fields. Do not do it for a plugin with a few knobs: the generic window already
has the assignment buttons, the preset list and a close button, and it is maintained by MOD.

Contents

1. [What MOD UI gives you, and what it expects back](#1-what-mod-ui-gives-you-and-what-it-expects-back)
2. [The files](#2-the-files)
3. [The parts the page must keep](#3-the-parts-the-page-must-keep)
4. [Why you cannot trust media queries](#4-why-you-cannot-trust-media-queries)
5. [Fit the screen with pages, never with scrolling](#5-fit-the-screen-with-pages-never-with-scrolling)
6. [JavaScript: one function, many instances](#6-javascript-one-function-many-instances)
7. [Hardware assignment: explain it to beginners](#7-hardware-assignment-explain-it-to-beginners)
8. [Touch, keyboard, accessibility](#8-touch-keyboard-accessibility)
9. [Styling rules](#9-styling-rules)
10. [Testing](#10-testing)
11. [Releasing: the cache](#11-releasing-the-cache)
12. [Checklist](#12-checklist)

---

## 1. What MOD UI gives you, and what it expects back

When the user opens the settings of a plugin, MOD UI (`html/js/modgui.js`):

* renders your **settings template with Mustache** and puts the result in a `<div class="mod-settings">`. That div is
  appended to `<body>`, positioned absolutely over the whole window (`body > .mod-settings`, a dark translucent
  background), and shown with `z-index: 1000`, above the pedals;
* gives the Mustache data `effect`, `brand`, `label`, `color` and `presets.factory` / `presets.user`
  (lists of `{uri, label, path}`);
* wires up some elements by **role attributes** it finds in the template (assignment buttons, the preset list, the
  close button);
* runs **your plugin's JavaScript** with `event.settings`, the `.mod-settings` element, next to `event.icon`.

So MOD UI owns the window, and your template owns what is inside it. Everything that follows is about filling that
box well.

## 2. The files

```
modgui.ttl                       modgui:settingsTemplate <modgui/<name>-settings.html> ;
modgui/<name>-settings.html      the template (Mustache)
modgui/<name>.css                the stylesheet of the icon AND of the settings page
modgui/<name>.js                 the JavaScript of the icon AND of the settings page
```

There is one stylesheet and one script for the plugin, shared by the icon and the settings page. A plugin that
declares `modgui:settingsTemplate` must also declare `modgui:stylesheet` and `modgui:javascript` as usual.

## 3. The parts the page must keep

Copy these from Hept7's `hept7-switch-settings.html` (or from MOD UI's own `html/resources/settings.html`); without
them the page looks fine and does not work.

| Part | Markup | What breaks without it |
|---|---|---|
| Root | one element that covers the window: `position: absolute; inset: 0` | the page is not where the user expects |
| Close | any element with class `js-close` | the user cannot leave; MOD UI closes the window on a click of it |
| Assignment buttons | `<button class="mod-address" mod-role="input-control-address" mod-port-symbol="Port_symbol">` | the control cannot be assigned to hardware. MOD UI adds the class `addressed` when it is; style that |
| Saved presets | the `.mod-presets` block with `{{#presets.factory}}` / `{{#presets.user}}` and its footer buttons | no factory or user presets |

Notes:

* The assignment button is the *whole* hardware-assignment feature: MOD UI opens its dialog when it is clicked. Make
  the button your own tile (a name, a "+" that becomes a check mark when `addressed`), but keep its attributes.
  `mod-port-symbol` is the port symbol of the TTL, which is saved in users' pedalboards: never rename it.
* The `.mod-presets` block is rewired by MOD UI once, right after rendering, by element references. **Moving
  these elements in the DOM afterwards is fine, replacing or cloning them is not** (see section 5).
* MOD UI's stylesheet styles these blocks (`.mod-presets`, `.btn`, ...). You will have to override widths, floats and
  heights, often with `!important`; do it inside your own namespace (`.h7s .mod-presets ...`).

## 4. Why you cannot trust media queries

The settings page is full screen, so `@media (max-width: 640px)` looks like the obvious way to adapt it to a phone. It
does not work in MOD UI:

* `index.html` of MOD UI has **no `<meta name="viewport">`**, and its stylesheet sets `body { min-width: 1024px }`.
* A phone or a tablet therefore lays the page out about **1024 CSS pixels wide and shrinks it** to the screen. A query
  on the viewport sees 1024 px, never 390. Everything looks like a tiny desktop.
* A plain narrow iframe, which is what a browser preview usually offers, *does* match the query. That is why the page
  can look responsive in a preview and not on the device. Do not conclude from a narrow preview window.

The way out, which Hept7 uses and which needs no change to MOD UI:

1. **Measure the box the page is drawn in** with JavaScript (`root.clientWidth` / `clientHeight`) and say what was
   found with attributes on the root (`data-w="s|m|l"`, `data-h="s|m"`), which the stylesheet uses instead of media
   queries. The same code can also measure what the content needs and choose a layout (section 5).
2. **Draw the page in the size of the real screen** on touch devices (`fitToScreen`): size the root box to as many
   CSS pixels as the screen has and scale it up to cover the window.

   ```js
   const touch  = window.matchMedia('(pointer: coarse)').matches;
   const device = layoutW > layoutH ? Math.max(screen.width, screen.height)   // iOS does not rotate screen.width
                                    : Math.min(screen.width, screen.height);
   const ratio  = touch ? device / layoutW : 1;            // layoutW: max(innerWidth, scrollWidth)
   if (ratio > 0.3 && ratio < 0.95) {                       // a shrunk page: draw it at the real size
     style.right = style.bottom = 'auto';
     style.width  = `${layoutW * ratio}px`;
     style.height = `${innerHeight * ratio}px`;
     style.transformOrigin = '0 0';
     style.transform = `scale(${1 / ratio})`;
   }                                                        // otherwise clear those styles
   ```

   Only touch screens that are really shrunk are touched: a desktop browser, zoomed or not, is left alone
   (browser zoom changes `innerWidth`, and must not be taken for a phone).

After this, a phone is a 390 x 844 page, and buttons of 48 px are 48 *real* CSS pixels, which is what the touch
guidelines talk about. (This could not be tested on a physical phone while writing this. If a device shows the page at
the wrong size, `fitToScreen` is the first suspect.)

## 5. Fit the screen with pages, never with scrolling

**The settings page must not scroll vertically.** A page that scrolls inside a window that is itself over a pedalboard
is easy to lose, hides controls, and fights the swipe gestures of a touch screen. If the content does not fit, split it
into **horizontal pages**, as the Anagram plugins do: a strip of pages, a row of dots saying which one is showing, and
arrows that are always visible.

The parts:

* **The strip** is a flex row that scrolls horizontally and snaps. A swipe works because it is native scrolling:

  ```css
  .h7s-pager { display: flex; overflow-x: auto; overflow-y: hidden; overscroll-behavior: contain;
               scroll-snap-type: x mandatory; scrollbar-width: none; }
  .h7s-pager::-webkit-scrollbar { display: none; }
  .h7s-page  { flex: 0 0 100%; overflow: hidden; scroll-snap-align: start; scroll-snap-stop: always; }
  ```
* **The controls** are in the top bar and are **always rendered**: previous, next (dimmed on the first and last page,
  not hidden) and one dot per page. The dots only show where you are; the arrows are the controls, because 44 px
  targets for ten dots do not fit on a phone. Arrow keys also change page. Say the page to screen readers
  (`aria-live`, "Page 2 of 5"), and set the page back after a re-layout.
* **The content is divided by purpose, not by count.** Hept7 has three kinds of panel (the scenes matrix, the hardware
  assignments, the saved presets); the assignments are further split into three small panels. A phone shows each one on
  its own page; a wide screen puts several on the same page. A panel that is too long for a page (seven preset rows,
  twelve tiles) is split in equal parts, and its title says *Scenes 2/3*.
* **The number of pages comes from measuring,** not from a table of devices. The script keeps all the panels in a hidden
  stock, and on every layout it tries arrangements from the richest to the plainest and keeps the first one that fits:

  1. put the panels on pages as the arrangement says;
  2. read back `scrollHeight <= clientHeight` (and the same for the width) of every page and panel;
  3. if something overflows, try the next arrangement; the last one splits panels (below) and always fits.

  To split a list, measure it with the panel alone on a page: room = page height - paddings - (panel height - list
  height); lines per page = `floor((room - head + gap) / (item height + gap))`, at least 1. Then divide the items in
  equal groups (never 6 + 1) and cut between lines (tiles are cut at a multiple of their columns).
* **The layout of one item adapts too.** A preset row is one line when its panel is at least 660 px wide, two lines at
  450 px, three below that. The class is chosen from the *panel's* width after it has been placed, because the
  same row is wider on a page of its own than next to a side column. The columns of a tile grid are computed the same
  way (`floor((width + gap) / (minTile + gap))`, capped, never more columns than tiles).
* **Short screens** (a phone on its side) get tighter spacing and lose the descriptions of the panels that are long,
  but never the 44 px size of a control.

Rules that make this safe:

* **Move DOM nodes, do not clone the ones that do something.** Moving keeps event handlers and what MOD UI wired up.
  Only static pieces (a panel's heading, the column titles of a matrix) are cloned for the continuation pages.
  Because the same elements are reused, look them up once and keep the references.
* **Use delegated events on `event.settings`** (`settings.on('click', '.x', ...)`) so a moved or re-created element
  still works.
* **Lay out when the window is visible and when it changes.** The window is created hidden. Observe `event.settings[0]`
  with a `ResizeObserver` (it also fires when the window is shown), and ignore a layout while
  `root.getClientRects().length === 0`. Coalesce with `requestAnimationFrame` and skip the work when the size is the
  same as the last one. Without `ResizeObserver`, fall back to `resize`.
* **Never re-layout while a text field has focus.** On a phone the on-screen keyboard resizes the window, and moving
  the field would close the keyboard. Remember that a layout is pending and run it on `blur`.
* **Keep the shadow of a panel inside the page.** The page is `overflow: hidden` to cut its content, which also cuts
  the shadow, leaving a hard edge. Let the strip reach the edges of the window and put the margins of the screen
  inside each page as padding (`margin: 0 calc(-1 * var(--pad))` on the strip, `padding: 4px var(--pad)` on the page),
  and keep the blur of the shadow smaller than that padding.

## 6. JavaScript: one function, many instances

The JavaScript of a modgui is **one function that serves every instance** of the plugin, for the icon and for the
settings page. Anything that changes lives in `event.data`, never in the closure.

```js
function (event, funcs) {
  if (event.type === 'start') {
    event.data.state = newState();
    buildSettings(event.settings);     // generate repetitive markup (7 rows x 9 cells) instead of writing it out
    cacheElements(event);              // look the elements up once ...
    assignEvents(event);               // ... and bind the delegated handlers once
    draw(event);
  } else if (event.data.state) {       // 'change' (a port or a parameter), 'patch_set'
    if (handle(event)) draw(event);
  }
}
```

* **One `draw(event)` paints the state on the icon and on the settings page,** and every handler is "update the state,
  `draw`, tell the plugin". That is what makes a name edited on the settings page appear on the pedalboard, in the top
  bar and on the assignment tiles at once. When you add a place that shows a value, add it to `draw`, not to the handler.
* **Show the result of a click immediately** and let the host's events have the last word. The host answers late,
  several clicks can be in flight, and MOD UI sends nothing when a port already holds the value, so a click may never
  be answered. Never let a late answer to an old click undo a newer one. Hept7's `selectPreset`, `hostSays` and
  `reconcile` are the worked example, and the preview's `scenarios.html` tests them.
* **Do not fight the user's typing.** Do not rewrite an input that has focus (`el !== document.activeElement`), sanitise
  what the plugin would sanitise (same rules as the C code), and repaint on `blur`.
* **Names the user can edit must show up everywhere the thing is named.** If presets have labels, the
  assignment tiles should say *"Layer 1"*, not *"Preset 1"* (the number can stay as a small second line). Keep a list of
  those elements and set their text in `draw`.
* Talk to the plugin with `funcs.set_port_value(symbol, value)`, `funcs.patch_get(uri)` and
  `funcs.patch_set(uri, type, value)`.
* Use `textContent` for anything that came from the user or the plugin, not `innerHTML`.

## 7. Hardware assignment: explain it to beginners

People who are new to the MOD Dwarf and its footswitches do not know what an assignment tile is for. Treat the
assignments as a topic of their own:

* **Group them by purpose** and give each group a panel: for Hept7, *Preset buttons* (one control per preset),
  *Current preset* (one control that steps through them) and *Screen pop-ups*. A flat list of twelve tiles says
  nothing.
* **Name the topic** on every group with a small label above the title (*Hardware assignment*), so that a page that
  shows only "Notify 1 to 4" still says what it is.
* **Add one or two sentences per group**: what it does, what to link to it, and any catch. Write them for someone
  who has not read the manual, in imperative or plain present, no jargon:

  | Group | Text |
  |---|---|
  | Preset buttons | Jump straight to a preset. Link one control of your device, such as a footswitch, to each preset: pressing it plays that preset. |
  | Current preset | Step through the presets with a single control: a knob, or the B+C pair of footswitches. Presets switched off in Cycle are skipped. |
  | Screen pop-ups | Show an alert with the preset name on the device screen whenever it changes. Link each one to a page of the device: it only appears on pages that have one. |

* **Say the catch where it bites.** Pop-ups only show on a device page that has the control addressed; the text above says
  so, instead of leaving the user to wonder why nothing appears.
* **Show the state on the tile**: a "+" while free and a check mark when `addressed`, in colour *and* shape.
* Keep the text on screens where there is room, drop it on short ones, and never drop it for a group that is alone on
  its page.

## 8. Touch, keyboard, accessibility

* **Targets of at least 44 px** (we use 44 to 56) on every button, switch and field, with `touch-action: manipulation`
  and no hover-only information.
* **Every control is a real `<button>` or `<input>`** with an `aria-label` or visible text; switches are
  `role="switch"` with `aria-checked`, toggle buttons use `aria-pressed`. State is shown by a mark as well as by colour.
* **Visible focus** (`:focus-visible`) on every control, and a logical tab order (the top bar first).
* **Respect `prefers-reduced-motion`**: no smooth page scrolling, no transitions.
* **Do not hide the way out or the page controls.** The close button and the arrows are in the top bar on every screen.
* `padding` with `env(safe-area-inset-*)` keeps controls out of the notch.

## 9. Styling rules

* **The stylesheet is global.** MOD UI appends it to the whole page, and runs it through Mustache. Namespace every
  class (`.h7s`, `.h7s-*` for the settings page; `.hept7`, `.h7-*` for the icon), and never write `{{`.
* **Zero-specificity resets** with `:where()`, so component rules win without `!important`:
  `:where(.h7s) :where(button, input) { appearance: none; margin: 0; border: 0; background: none; font: inherit; }`.
* **Custom properties** for colours and spacing, declared on the root (`--h7-ink`, `--h7s-pad`).
* **Use `color-mix()` only after a plain fallback** declaration.
* **Do not rely on MOD UI's film or knob widgets** in the page; draw the controls yourself.
* On the **icon** (not the settings page), MOD UI covers the plugin with `.mod-drag-handle` (`z-index: 20`) so it can be
  dragged. Whatever must be clickable on the icon needs `position: relative; z-index: 30`. An overlay on the icon
  needs a higher one with `pointer-events: none`. The settings window sits above everything (`z-index: 1000`).
* **No vertical scrolling in the root** (`overflow: hidden`); the only scrolling things are the pager (sideways) and
  the saved-presets list (which has an unknown number of items).

## 10. Testing

Use the preview tool (`tools/modgui-preview`, see its README):

```bash
python3 tools/modgui-preview/serve.py        # http://localhost:8000/?plugin=<name>
```

* Open the **Settings page** view and try every **Screen** size, then the **As MOD UI shows it** buttons (phone, phone on
  its side, tablet). Those reproduce the 1024 px page shrunk to the screen, which a plain narrow frame cannot, and
  are the ones that matter for section 4.
* Go through all the pages with the arrows and with the keyboard; check the first and the last (dimmed arrows, the
  dots), a name that is 15 characters long, and the preset list with user presets.
* Look at the **red bar** the preview shows when something covers a control (the clickable check), and at
  `scenarios.html` if the plugin selects a preset at a time.
* Measure, do not just look: for each size, count the pages and check that no control sticks out of its page. The
  preview can be driven from a script (headless Chrome with a DevTools connection and `Page.captureScreenshot`) for
  this, which is how the Hept7 sizes from 360x640 to 1920x1080 were checked.
* **Still try it on a real device and a real pedalboard** before calling it done: the stacking over the pedals, the
  scale on a physical phone and the HMI are not covered by the preview.

## 11. Releasing: the cache

MOD UI serves modgui files with `Cache-Control: max-age=31536000` and its URL only changes with the plugin version
(`?v=builder_micro_minor_release`). **Raise `lv2:microVersion` in the plugin TTL whenever the template, CSS or JS
changes**, or the device keeps running the old page: that is the usual reason a fix "does nothing". During
development use odd numbers (LV2 reads them as nightly).

Also: regenerate the screenshots and the manual PDF in the commit that changes the page, and describe in the manual
what the user sees (pages, swipe, the arrows).

## 12. Checklist

**Structure**

- [ ] `modgui:settingsTemplate` in `modgui.ttl`; a `js-close` element; the assignment buttons with `mod-role` and
      `mod-port-symbol`; the `.mod-presets` block
- [ ] Every class namespaced, no `{{` in the CSS, zero-specificity resets
- [ ] One JavaScript function; state in `event.data`; one `draw`

**Fitting the screen**

- [ ] No `@media` on the viewport for layout; sizes come from measuring the page
- [ ] `fitToScreen` for touch screens; checked with the "As MOD UI shows it" buttons
- [ ] No vertical scrolling; content split into pages; arrows and dots always visible; swipe and arrow keys work
- [ ] The layout runs on show and on resize, not while a field has focus, and keeps the current page
- [ ] Panels' shadows are not cut at the edge of a page

**Content**

- [ ] Controls grouped by purpose, each group with a title and a sentence for beginners; the topic named
      (*Hardware assignment*)
- [ ] Names edited by the user show everywhere (icon, top bar, assignment tiles)
- [ ] Targets of 44 px or more, real buttons, ARIA state, visible focus, reduced motion

**Release**

- [ ] `lv2:microVersion` raised; screenshot, manual and PDF updated
- [ ] Tried on a real pedalboard, or said that it was not
