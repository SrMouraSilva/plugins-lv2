# Writing an HTML manual for a plugin

`modgui:documentation` can point to an **HTML file**, and MOD UI opens it in a new tab from the plugin's page. This
repository writes each manual once, in Markdown (`docs/manual.md`), and builds a single, self-contained page from it
(`modgui/manual.html`) with a cover, a side navigation, numbered steps, callouts, video cards, collapsible questions, a
dark theme and a print style. This guide is for anyone who wants the same for their plugin.

Contents

1. [Why a self-contained page](#1-why-a-self-contained-page)
2. [Files](#2-files)
3. [Building](#3-building)
4. [`manual.json`](#4-manualjson)
5. [Writing `manual.md`](#5-writing-manualmd)
6. [Pictures for the steps](#6-pictures-for-the-steps)
7. [Using it from the TTL](#7-using-it-from-the-ttl)
8. [Checking](#8-checking)

---

## 1. Why a self-contained page

MOD UI serves the documentation at `/effect/file/documentation?uri=<plugin uri>`: whatever the page links to with a
relative path (`images/x.webp`, `style.css`) is looked up at `/effect/file/images/x.webp`, which does not exist. So the
builder puts **everything inside the one file**: the CSS, the JavaScript and every image (as `data:` URIs; screenshots
over 150 KB are re-encoded as WebP). It also works offline and when the file is attached to an e-mail.

Keep the images small (a 2x screenshot of the plugin is enough) and **use WebP** for screenshots and drawings: they need
the transparency that JPEG lacks, and WebP is about a fifth of the size of PNG (`python3 tools/media/to_webp.py FILE.png`;
the tools of `tools/media/` already write WebP). A PNG or JPEG over 150 KB is re-encoded by the builder anyway and use the GIFs of [making-gifs.md](./making-gifs.md)
only where they tell something: they are the largest part of the file (a manual is typically 1 to 3 MB).

## 2. Files

```
tools/manual/build_manual_html.py     the builder
tools/manual/manual.css, manual.js    the look and the navigation (shared by every plugin)
plugins/resources/<name>.lv2/docs/manual.md      the text (the single source of truth)
plugins/resources/<name>.lv2/docs/manual.json    the title, the tagline and the colours of the plugin
plugins/resources/<name>.lv2/docs/images/        the pictures the text uses
plugins/resources/<name>.lv2/modgui/manual.html  the result (committed: it is what the device serves)
```

To use it in another repository, copy `tools/manual/` and change `RESOURCES` at the top of the builder.

## 3. Building

```bash
pip install markdown pillow          # Pillow is only used to shrink big screenshots and to read image sizes
python3 tools/manual/build_manual_html.py tetr4-switch      # one plugin
python3 tools/manual/build_manual_html.py all               # every bundle that has docs/manual.md
```

Rebuild in the commit that changes the manual **or the interface** (screenshots).

## 4. `manual.json`

```json
{
  "title": "Tetr4 Switch",
  "eyebrow": "MOD Audio · Plugin manual",
  "tagline": "Four scenes. One stomp.",
  "subtitle": "Programmable CV loop switcher for MOD Dwarf, MOD Duo X and MOD Duo",
  "badges": ["4 presets", "4 CV outputs", "16 DIP switches"],
  "edition": "User manual · November 2026 edition",
  "version": "v1.0.0",
  "hero": "images/panel.webp",
  "brand": "#0a1380",  "brand2": "#02094e",
  "accent": "#2b41c4", "accentDark": "#a9b6ff",
  "colors": ["#ff6667", "#ffff67", "#66ffff", "#bb7cd2"],
  "links": [{"label": "Forum thread", "url": "https://..."}]
}
```

| Field | Use |
|---|---|
| `title`, `tagline`, `subtitle`, `badges`, `eyebrow`, `edition`, `version` | the cover (all optional; the title falls back to the first `# ` of `manual.md`) |
| `hero` | the picture on the cover, relative to `docs/` (a 2x screenshot of the plugin; default `../modgui/screenshot.png`) |
| `brand`, `brand2` | the cover gradient, and the colour of headings and table headers on the light page |
| `accent` | links and marks on the light page: **dark enough to read on white** (contrast 4.5 or more) |
| `accentDark` | the same on the dark page (it follows the system theme): a light tint of the brand |
| `colors` | the colours of the plugin (one per preset or channel): the strip under the cover, the headings and the recipe grids |
| `links` | shown under the side navigation |
| `output` | where the page is written, relative to the bundle (default `modgui/manual.html`, which is installed on the device and is what `modgui:documentation` points to). Use `docs/manual.html` for a page the TTL does not use yet: `docs/` is not installed |

## 5. Writing `manual.md`

Plain Markdown, plus a few conventions that the builder turns into components. The text stays readable on GitHub.

* **`# Title`** at the top is the cover. Every **`## Heading`** is a section (a card, and an entry of the navigation).
  A number in front (`## 3. The controls`) becomes the small `03` above the title; without it there is none. What comes
  before the first `##` is the introduction.
* **Numbered lists are steps**: a numbered circle per item, joined by a line. Use them for procedures only. A picture or
  a second paragraph inside a step needs care (see *A picture inside a step*, below).
* **Callouts**, with GitHub's alert syntax and an optional title: `> [!NOTE] Good to know`, `[!TIP]`, `[!WARNING]`,
  `[!IMPORTANT]`. A plain `>` quote is a neutral callout.
* **Figures**: an image alone in a paragraph, `![caption](images/x.webp)`; the alt text is the caption. A picture narrower
  than 760 px is not stretched (a phone screenshot, a GIF); wider ones fill the column; click to enlarge.
* **Videos**: `[![Title — description](images/thumb.jpg)](https://youtu.be/ID)` (a link to YouTube around a thumbnail; the
  " — " separates the title from the description) becomes a card with a play button. The same with another address is a
  card without the button (an example pedalboard, a store page).
* **Questions**: in a section whose title contains *Questions* or *FAQ*, every `###` heading and what follows it becomes a
  collapsible question.
* **Parameter sheets**: Markdown definition lists (`TERM` on a line, `: text` on the next) become a tinted sheet with the
  terms in small capitals.
* **Tables** get a coloured header and zebra rows; the first column is bold. Use `<span class="dot" style="--c:#ff6667"></span>`
  for a colour swatch and `<span class="num">1</span>` for a small numbered badge.
* **Recipes** and **pictures with numbered pins** are blocks of their own (see the examples below).
* Links to a section: `[Recipes](#recipes)` (the id is the heading in lowercase, with dashes; GitHub's own anchor of a
  numbered heading, `#6-recipes`, works too).

Write for someone who has never used the plugin: say what to do before saying how it works, show the result of every
step, and put the one thing they must not miss in a callout.

### A picture inside a step

A picture or a second paragraph inside a step is indented by **four** spaces and has a blank line before and after it
(with three, or without the blank lines, the list breaks and the next numbers become plain text):

```markdown
4. **Set the DIP switches.** In Config 1, set all four switches down.

    ![Step 4: Config 1 all down](images/step-dip-switches.webp)

5. **Click the footswitches.** ...
```

### A recipe

A grid of switches and a text, for plugins with switches or steps. A row is a name and one `1` (on, in the colour of that
row) or `0` per column:

````markdown
```recipe
Four-scene guitar rig
cols: 1 2 3 4
Clean: 0000
Modulation: 0011
---
What the recipe does, in a paragraph.
assign: **Assigning the plugin:** Out 1 → Distortion
```
````

### A picture with numbered pins

Raw HTML, with the position of each pin in percent of the picture. Describe the numbers in a table with
`<span class="num">1</span>` in the first cell:

```html
<figure class="annotated">
<div class="stage">
<img src="images/panel.webp" alt="The panel">
<span class="pin" style="--x:24.8%;--y:16.4%">1</span>
</div>
<figcaption>What the numbers mean.</figcaption>
</figure>
```

## 6. Pictures for the steps

A step is much easier to follow with a picture of what to tap. `tools/media/shots.mjs` takes them from the modgui preview
(see [making-gifs.md](./making-gifs.md) for the preview and the finger), so they are **reproducible and stay in step with
the interface**: when the interface changes, run it again.

```bash
node tools/media/shots.mjs all        # every picture of every plugin
node tools/media/shots.mjs gossiper   # the pictures of one plugin
```

Each picture is one entry in `tools/media/shot-specs.mjs`: the plugin is put in the state of the step (`setup`), the
target gets a ring (`highlight`), the finger points at it (`finger`) and the picture is cropped to a panel or to the plugin
(`crop`). For the settings page, `scrollTo` brings the page of the pager that holds the target into view. Example:

```js
// Step 6: assign the presets to device footswitches
settings('step-assign-preset-buttons.webp', '[mod-port-symbol="Preset_selector_1"]',
         { highlight: ['[mod-port-symbol="Preset_selector_1"]', '[mod-port-symbol="Preset_selector_2"]'] }),
```

Pictures are written to `docs/images/` of the bundle. Take them for the steps that happen **in the plugin's interface**;
steps that happen in MOD UI's own dialogs (the pedalboard's CV ports, the assignment dialog) are not in the preview and
need a screenshot of MOD UI itself. Do not redo those: they do not change with your plugin.

## 7. Using it from the TTL

```turtle
modgui:documentation <modgui/manual.html> ;
```

Raise `lv2:microVersion` when the manual changes: MOD UI caches the files of a modgui for a year, keyed by the version of
the plugin. Keep `docs/manual.md` as the source: do not edit `manual.html` by hand (it says so in its first comment).

The page prints well (A4, the cover on its own, navigation hidden, the questions open). To make a PDF from it:

```bash
google-chrome --headless=new --print-to-pdf=manual.pdf --no-pdf-header-footer plugins/resources/my-plugin.lv2/modgui/manual.html
```

## 8. Checking

Open the file in a browser and look at it at **three widths** (phone 390 px, tablet 820 px, desktop 1440 px), in the light
and in the dark theme of the system: the cover, one table, a step list, a callout, a video card and every picture. Pictures
must keep their proportions at every width (the cover picture is the usual suspect: it must never be stretched). Open the
links: a manual full of dead links is worse than none.
