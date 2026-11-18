# An interactive manual: the plugin inside its own manual

A manual built with [html-manuals.md](./html-manuals.md) can have one special section in which **the plugin itself is loaded**: the same
interface, the same script, running in a frame of the page. The reader can

* **Explore**: tap any part of the plugin and read what it is for, switch between the plugin **on the pedalboard** and its **settings
  page**, and outline everything that can be tapped (*Show areas*);
* take a **guided tour** of a common task: a finger (a semi-transparent circle, like the one of the [animations](./making-animations.md))
  glides to what must be tapped, taps it, and a description says what is happening. A tour is taken **one step at a time**
  with *Next* and *Back*.

It is drawn by JavaScript, not recorded: nothing is a GIF, and the plugin really answers (switches move, names are typed, the settings page
changes its pages). This guide is for anyone who wants the same for their plugin.

Contents

1. [How it works](#1-how-it-works)
2. [Files](#2-files)
3. [Adding it to a manual](#3-adding-it-to-a-manual)
4. [`interactive.json`](#4-interactivejson)
5. [Writing the areas](#5-writing-the-areas)
6. [Writing a tour](#6-writing-a-tour)
7. [What is real and what is simulated](#7-what-is-real-and-what-is-simulated)
8. [Checking](#8-checking)
9. [Limits](#9-limits)

---

## 1. How it works

The build (`tools/manual/build_manual_html.py`) puts three things in `modgui/manual.html`, which stays **one self-contained file**:

1. **The page of the frame**, as a JSON string (`interactive.py`: `frame_document`). It holds the **real files of the bundle**, read at
   build time: the icon and the settings template (rendered with a small Mustache, with the ports and the presets of the TTL, as
   MOD UI does), the stylesheet and the JavaScript of `modgui/`, and a stand-in for MOD UI: `tools/manual/interactive/host.js`
   and `host.css`, plus jQuery 1.9.1, the version MOD UI itself runs the scripts with. **Nothing is copied or redrawn**, so the
   interactive part cannot fall behind the interface: rebuild the manual and it follows.
2. **The data** (`docs/interactive.json`): the areas that can be tapped and the tours.
3. **The page around the frame** (`panel.js`, `panel.css`): the buttons, the description card, the tours and their player. It is
   added to the manual only when it has an interactive block.

At run time the frame is a **same-origin `srcdoc` iframe**, loaded when the section comes near the screen (a manual is long and most readers
never reach it). The frame isolates the two stylesheets from each other (the stylesheet of a plugin is global) and gives the settings page the viewport
it expects, so its layout by pages works exactly as on a device. The page and the frame talk directly (`frame.contentWindow.Host`).

`host.js` plays the part of MOD UI for the script of the plugin: it sends the `start` event with the ports and the parameters, gives it
`funcs` (`set_port_value`, `patch_get`, `patch_set`...), opens and closes the settings page like the window of MOD UI, and offers the page
around it a small API: `view`, `tap`, `type`, `ring`, `showAreas`, `setAreas`, `handlers.select`. Everything it draws on top (the ring, the
outlines, the finger) is in an overlay of its own, so none of it is part of the plugin.

## 2. Files

```
tools/manual/interactive.py             the Mustache, the frame, the JSON (called by build_manual_html.py)
tools/manual/interactive/host.js, .css  the stand-in for MOD UI, inside the frame
tools/manual/interactive/panel.js, .css the page around the frame (Explore, Guided tours)
tools/manual/interactive/vendor/        jQuery 1.9.1 (MIT) and MOD UI's own jack and button pictures (vendor/mod-ui, AGPL-3.0); both in NOTICE
plugins/resources/<name>.lv2/docs/interactive.json   the areas and the tours of the plugin
plugins/resources/<name>.lv2/docs/manual.md          holds the ```interactive block
```

To use it in another repository, copy `tools/manual/` (it needs the TTL reader of `tools/modgui-preview/ttl.py`: change `sys.path` at the top of
`interactive.py`).

## 3. Adding it to a manual

Give the section a heading and a short introduction, and put an empty `interactive` block where the plugin goes (the block is replaced by the
component; the text around it is yours):

````markdown
## 4. Interactive manual

Here the plugin is **the real one**. Tap any part of it to read what it does, or follow a guided tour.

```interactive
```
````

Then write `docs/interactive.json` (next section) and build as usual: `python3 tools/manual/build_manual_html.py <plugin>`.

A section with a number changes the numbers of the sections after it, and the manual may refer to them by number (`section 4`): search the
text before inserting one. Put the section after the one that describes the controls: whoever has just read about them wants to try.

The manual costs about 170 KB more (jQuery, the stylesheet and the script of the plugin, the data). On paper (and in the PDF) the block is not
printed: it says that the part is interactive.

## 4. `interactive.json`

```json
{
  "views": [{"id": "pedalboard", "label": "On the pedalboard"}, {"id": "settings", "label": "Settings page"}],
  "areas": [
    {"id": "foot", "view": "pedalboard", "select": ".t4-foot", "title": "Footswitch of preset {preset}",
     "chip": "Footswitches", "kicker": "On the pedalboard", "text": "Selects preset {preset}...",
     "links": [{"label": "Hands-free control", "href": "#hands-free-control"}]}
  ],
  "tours": [
    {"id": "two-scenes", "title": "Two scenes", "summary": "One line for the card.",
     "steps": [
       {"view": "pedalboard", "title": "Config 1: everything off", "ring": ".t4-bank",
        "do": [{"tap": ".t4-dip[data-preset=\"1\"][data-output=\"1\"]"}],
        "text": "Put switch **1** of Config 1 **down**."}
     ]}
  ],
  "text": {"intro": "Tap any part of the plugin..."}
}
```

| Field | Use |
|---|---|
| `views` | Optional. By default `pedalboard`, and `settings` when the bundle has a `modgui:settingsTemplate`. The labels are the buttons of the page |
| `areas` | What a tap says (section 5). The **order matters**: a tap is given to the first area that matches, so write the specific ones before the general ones |
| `tours` | The guided tours (section 6) |
| `meters`, `midi` | Optional: a box beside each CV jack with its voltage, and one beside a MIDI jack with its messages (see below) |
| `text` | Optional: any of the words of the page (`TEXT` in `interactive.py`), to translate or rephrase |
| `frameTitle` | Optional: the title of the frame for a screen reader |

`meters` makes the **manual** (not MOD UI) write the voltage of each CV output beside its jack, so it is clear when it is 0 V and when 10 V. It is
`{"select": ".mod-output[mod-role=\"output-cv-port\"]", "on": "Preset_{active}_output_{n}", "volts": 10}`: `select` picks the jacks, `{n}` is the place of the
jack (1, 2...), `{active}` the preset the simulated host is playing, and the output is on (`volts`) when that port is 1, else 0 V. It follows the ports
as the plugin changes them. Say in the area of the jacks that the box is the manual's (the plugin does not draw it).

`midi` does the same for a **MIDI out**: a box beside the jack lists, in order, the messages the plugin would send. It does what the plugin's C code does, so it must be
kept in step with it: `{"select": "<the MIDI jack>", "count": 4, "channel": "MIDI_channel", "changesOnly": "MIDI_changes_only", "cc": "MIDI_CC_{n}",
"on": "Preset_{active}_output_{n}"}`. When the preset **changes**, for each output `n` (1 to `count`) it sends the control change `cc` on `channel` with 127 if the
`on` port is 1 and 0 if not; with `changesOnly` it skips the outputs whose value equals the last one sent (all start at 0). Nothing is sent at load or when a
switch moves. Say in the area of the jack that the box is the manual's, and give a tour step that looks at it (`"ring": ".ix-midi"`).

`title`, `chip` and `kicker` are one line, and `text` is Markdown (it becomes HTML at build time). Inside any of them, `{name}` is replaced by the
`data-name` of the part that was tapped (`{preset}` and `{output}` for `data-preset` and `data-output`), and `{title}` by its `title`
attribute: one area describes all the switches of a kind and still says which one it was.

## 5. Writing the areas

An **area** is one kind of part. `select` is a CSS selector, looked up from what was tapped upwards (`closest`), in the view that is showing:

* the classes of the **plugin's own** interface (`.t4-dip`, `.t4s-pad`) and of what MOD UI adds (`.mod-actions .mod-settings`, the jacks:
  `.mod-output[mod-role="output-cv-port"]`);
* list the specific parts before the container that holds them (a switch before its bank, a bank before the scene card), or the container eats every tap;
* the parts that can be tapped appear as numbered **chips** under the picture (so the page works without hovering and with a keyboard), and as
  numbered pins with *Show areas*. The number is the place in the list **of that view**. `chip` is the short name for the chip (it falls back to the title).

Say what it is **for**, then how it behaves; add `links` to the section of the manual that goes deeper. Nothing is hover-only.

Tapping is not blocking: the part works as it does in the plugin **and** the description appears. That is why the page says that nothing is saved or
sent, and has a *Reset* button.

## 6. Writing a tour

A tour is a **common task**, from the quick start or the recipes of the manual: five to seven steps, each saying one thing. Each step has

| Field | Use |
|---|---|
| `title`, `text` | What the reader reads: the title is one line; the text is Markdown, one or two sentences, in the present tense ("Tap **Preset 2**. The bank lights up") |
| `view` | The view to show (`pedalboard`, `settings`). It changes only when it differs from the current one, so write it on the first step and where it changes |
| `ring` | The part the step is about, ringed in yellow before the finger arrives. By default, the target of the first action; `false` for none. Can be `{"select": "...", "has": "text"}` |
| `do` | The actions, in order |

Actions:

| Action | Does |
|---|---|
| `{"tap": "selector"}` | The finger glides to the first **visible** element that matches, presses, and the page gets the clicks and pointer events of a touch |
| `{"tap": "selector", "has": "text"}` | The same, for the first one whose text holds `text` (a factory preset by its name: its position is not a good selector) |
| `{"type": "selector", "text": "Lead"}` | Taps a text field and types into it, one letter at a time (the plugin gets real `input` events) |
| `{"wait": 500}` | A pause, in ms |
| `{"view": "settings"}` | Changes the view in the middle of a step |

If the target is on another page of the settings page's pager, the host scrolls it into view first, as a swipe would. MOD UI's assignment dialog is
drawn by the host in a simplified way: a tap on an assign button opens it, and `{"tap": ".ix-ok"}` confirms it.

How the player works, so you can write for it:

* **Next** shows the next step with the finger. **Back** and the numbers of the list **start from the clean plugin and do at once what the steps before did**
  (and the step itself, without the finger): the frame always agrees with the description. So the steps of a tour must be **replayable from the
  start**, and must not depend on something the reader did before.
* The reader is always in control: nothing advances by itself, so a step can be read for as long as needed. While a tour is open a shield covers the
  frame, so only the tour taps. On the last step the reader can *Start again* or *Try it yourself* (Explore, with the state the tour left).
* A tour starts from the plugin as it loads: the defaults of the TTL. Write the first step for that state.
* Prefer **idempotent-looking** taps (a switch that is up and must be down) and say in the text what the reader should see.
* Do not show what the plugin does not do: an output that lights up, a pop-up. The host only simulates what section 7 lists.

## 7. What is real and what is simulated

| Real | Simulated by the host (as `tools/modgui-preview/frame.html` does) |
|---|---|
| The icon, the settings page, the stylesheet and the script of the plugin | The **ports and parameters**: they only hold the values they are given |
| The layout of the settings page by pages, its swipe and its arrows | The **host** of plugins with `Preset_selector_N` ports: MOD UI sends nothing for a value the port already has; after 150 ms the previous selector goes off, the new one on, and `Preset_combobox` moves (leaving out the port the change came through, as mod-host does) |
| The factory presets: choosing one loads every port and every name | The **assignment dialog** of MOD UI: a drawing, and *Assign* marks the button `addressed` |
| Typing, the sanitising of names, the limits of the numbers | The **jacks and the gear** around the plugin: MOD UI's own jack pictures (and the icons of the preset buttons), laid out by `host.css` as its dashboard does |

The C side, the device display, the CV and MIDI signals are **not** there: a manual that shows what the output of a plugin does needs its own drawing in the
description. A plugin whose script depends on something else from the host (a monitored output, for example) needs that added to `host.js`:
look at `hostSelects`, and keep it generic by what the TTL has (the ports), not by the name of a plugin.

## 8. Checking

Build, open `modgui/manual.html` in a browser (also from `file://`: it must work offline) and go to the section. Check, at **phone (390), tablet (820)
and desktop (1440) widths**, light and dark:

* the frame loads, the two views show, *Show areas* outlines what the chips list, a tap on each part says the right thing;
* every tour, with **Next** from step 1 to the end, then **Back** to the middle: the description always agrees with the frame;
* with **prefers-reduced-motion** the finger does not glide and the ring does not pulse.

Scratch scripts that drive the page with Chrome must use `launch()` of `tools/media/cdp.mjs` with `try/finally { page.close() }` (see
[making-animations.md](./making-animations.md#9-cleaning-up-after-chrome)): the page is in the `.ix-frame`, so
`document.querySelector('.ix-frame').contentWindow.Host` is the way in (`Host.log` has what the plugin sent to the host; `Host.tap(selector)`
and `Host.type(selector, text)` do what the player does).

Raise `lv2:microVersion` of the plugin and the version of the manual when the manual changes, as for any manual change.

## 9. Limits

* The frame is **at most as faithful as the host**: a plugin whose script needs more of MOD UI than `host.js` gives (pop-ups, the Performance view)
  shows what it can. Anything the script needs and the host lacks shows as an error in the console of the frame.
* A phone shows the pedalboard view small (the plugin is 600 px wide): the page says so and suggests the settings page, which is made for touch.
* Sound is not part of it: nothing here makes a signal.
