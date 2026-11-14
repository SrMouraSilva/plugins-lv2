# Making an alternative thumbnail

MOD UI uses two pictures of a plugin: `modgui:screenshot` (about 500 px wide, the plugin itself) and `modgui:thumbnail`
(about 150 px wide). Marketplaces and plugin stores often show **a third, simpler picture** first: a small abstract
drawing that says what the plugin *is* at a glance, instead of a reduced copy of its interface. This guide explains how
this repository makes it (`alternative-thumbnail.html` and `alternative-thumbnail.webp`, one pair per plugin) so that you
can do the same.

MOD UI does not read it, so it costs nothing on the device: the files live in the bundle's `docs/` folder, which is not
installed. It is for the Readme, a store page, a forum post or a slide.

Contents

1. [What it is, and what it is not](#1-what-it-is-and-what-it-is-not)
2. [The specification](#2-the-specification)
3. [How to design it](#3-how-to-design-it)
4. [A starting point](#4-a-starting-point)
5. [Rendering](#5-rendering)
6. [Checklist](#6-checklist)

---

## 1. What it is, and what it is not

| `thumbnail.png` | `alternative-thumbnail.webp` |
|---|---|
| A reduced picture of the real interface | A drawing of the *idea* of the plugin |
| Needs the whole interface, with its text | **No text at all** |
| Unreadable at 150 px | Readable at 100 px |
| Made from the plugin's own CSS | Made from a few flat shapes in the colours of the plugin |

The test: show it, small, to someone who has never seen the plugin. They should guess the kind of thing it is (four
switches and four footswitches: a switcher; a display and seven buttons: presets; a display, dots and meters: a
monitor), not read a label.

## 2. The specification

* **400 × 400 px**, a rounded square (corner radius 88 px), **transparent outside the corners**.
* **WebP, not PNG.** The picture needs an alpha channel (the corners), which JPEG does not have; PNG has it but is about five
  times heavier for flat colours and gradients (this repository: 70–100 KB as PNG, 9–11 KB as WebP). WebP keeps the
  transparency, is read by every browser, by GitHub's Readme and by the usual stores' pages. Render at quality 96: lower
  shows banding in the gradients. (`thumbnail.png` and `screenshot.png`, the two that **MOD UI** reads, stay PNG: they are
  part of the plugin's TTL and MOD's own tools expect that format.)
* A background that carries the colour of the plugin (the colour of `modgui:color`, with a gentle gradient: a strong one
  looks dated), and 3 to 6 shapes drawn on it.
* **No text, no logo, no real UI.** Numbers and labels do not survive the small size and cannot be translated.
* Self-contained HTML and CSS (no images, no fonts, no scripts): the file must render the same anywhere.
* Files: `plugins/resources/<plugin>.lv2/docs/alternative-thumbnail.html` (the source) and
  `alternative-thumbnail.webp` (the result, committed).

## 3. How to design it

1. **Pick the one idea.** What is the plugin *made of*? A switcher is made of switches and footswitches; a monitor of a
   display and meters. Draw only that.
2. **Use the colours of the plugin**: take the CSS variables of its stylesheet (`--t4-c1`...), so the picture and the
   interface agree. If the plugin has a colour per preset or per channel, use them: they are the quickest way to say
   "several of the same thing".
3. **Show state, not just parts.** One lit LED, one switch up, one button pressed, one meter half full. A picture where
   everything is off looks dead; a picture where everything is on says nothing.
4. **Simplify the shapes**: rounded rectangles, circles, pills. A button is a rounded rectangle with a lighter top; a
   display is a dark rounded rectangle with a glowing square. Add one soft shadow or glow per lit element, not more.
5. **Use space.** Leave about 44 px of margin; group shapes in two or three rows or columns on a clear grid.
6. **Check the contrast at small size** (section 5), in light and dark surroundings: the picture is transparent outside
   the corners, so it is shown on whatever the page has.

The three of this repository are good references: `tetr4-switch.lv2/docs/alternative-thumbnail.html` (four DIP switches
over four footswitches), `hept7-switch.lv2/docs/alternative-thumbnail.html` (a display with eight squares over seven
buttons) and `gossiper.lv2/docs/alternative-thumbnail.html` (a display, seven discs and four meters).

## 4. A starting point

```html
<!doctype html>
<!-- An abstract picture of My Plugin, 400x400, for internal use. Rendered by tools/media/thumbnails.mjs -->
<meta charset="utf-8">
<style>
  html, body { margin: 0; background: transparent; }
  .card {
    position: relative; box-sizing: border-box; width: 400px; height: 400px;
    border-radius: 88px; overflow: hidden;
    background: radial-gradient(90% 70% at 15% 0%, rgba(255, 255, 255, .14), rgba(255, 255, 255, 0) 60%),
                linear-gradient(170deg, #0a1380, #02094e);               /* the colour of the plugin */
    box-shadow: inset 0 0 0 4px rgba(255, 255, 255, .08);                 /* a thin edge */
  }
  /* A dark slot with a knob in it (a switch): up = on */
  .slot { position: absolute; top: 74px; width: 62px; height: 150px; border-radius: 22px; background: #040a40;
          box-shadow: inset 0 5px 9px rgba(0, 0, 0, .7); }
  .slot::before { content: ""; position: absolute; left: 7px; right: 7px; top: 8px; height: 58px; border-radius: 16px;
                  background: linear-gradient(#fff, #c9cffc); box-shadow: 0 5px 7px rgba(0, 0, 0, .5); }
  .slot.on { background: #ff6667; box-shadow: inset 0 5px 9px rgba(0, 0, 0, .3), 0 0 30px -4px #ff6667; }
</style>
<div class="card">
  <div class="slot on" style="left: 40px"></div>
  <div class="slot" style="left: 124px"></div>
</div>
```

Keep one rule per kind of shape and reuse it (as `.slot` above); put the position and the colour of each instance in
its `style` attribute.

## 5. Rendering

```bash
node tools/media/thumbnails.mjs        # every bundle that has docs/alternative-thumbnail.html
```

It opens each file in headless Chrome at 400 × 400 with a transparent background and saves the WebP next to it (Node 22+
and Google Chrome; `CHROME_BIN` if Chrome is not in the usual place). Without the script: Chrome's command line can only
write PNG, so render a PNG and convert it (`pip install pillow`):

```bash
google-chrome --headless=new --disable-gpu --default-background-color=00000000 --window-size=400,400 \
  --screenshot=alternative-thumbnail.png file://$PWD/alternative-thumbnail.html
python3 tools/media/to_webp.py alternative-thumbnail.png      # writes alternative-thumbnail.webp and removes the PNG
```

Then **look at it small**: put the pictures of all your plugins side by side, reduced to 100 px, on a white and on a
dark background. Show it in the Readme next to the title of the plugin:

```html
<img src="./plugins/resources/my-plugin.lv2/docs/alternative-thumbnail.webp" alt="My Plugin" width="120" align="right">
```

## 6. Checklist

- [ ] 400 × 400, rounded square, transparent corners
- [ ] No text; self-contained HTML/CSS
- [ ] The colours of the plugin's stylesheet; one lit element so it looks alive
- [ ] Still clear at 100 px, on a light and on a dark page
- [ ] WebP with transparency (quality 96), not PNG
- [ ] `alternative-thumbnail.html` and `.webp` committed together; shown in the Readme
