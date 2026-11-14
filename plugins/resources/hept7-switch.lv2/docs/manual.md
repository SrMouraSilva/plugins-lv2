# Hept7 Switch — User Manual

Hept7 Switch turns **one footswitch press into eight CV signals at once**. You
choose, for each of its **7 presets**, which of its **8 CV outputs** are on
(10 V) or off (0 V), and then change between presets with a footswitch, a
knob or the screen. Route the CV outputs to the on/off (or any CV-addressable)
parameters of your plugins and a single press can turn a whole scene on.

It is the bigger sibling of Tetr4 Switch, and it has two things of its own:
a **display** that shows the preset (and announces every change of it), and a
**full-screen settings page** made for touch screens.

![Hept7 Switch on the pedalboard](images/hept7-pedalboard.png)

## 1. The plugin on the pedalboard

| Item | What it does |
|---|---|
| **Display, big number and name** | The active preset. **The name is editable**: click it and type (up to 15 characters). It is also the text the device screen pop-up shows (see section 4). |
| **Display, eight squares** | The CV outputs that are on in the active preset. **They are buttons**: click one to turn that output on or off in the active preset, without opening the settings page (the other presets are edited on the settings page). |
| **Seven buttons** | Select the preset. The button of the active preset lights up in its colour. There are seven because the device footswitch LEDs have seven colours (red, yellow, green, cyan, blue, magenta, white): each preset has its own, here and on the device. A button with its number **crossed out** is a preset left out of the cycle of `Current preset` (section 3); the button still selects it. |
| **Edit scenes** | Opens the settings page (section 2). The gear icon above the plugin opens it too. |
| **Eight CV jacks** | `CV Output 1`–`8`, on the back plate at the right, in two columns (1–4 and 5–8). |

### The display announces every change of preset

Whenever the preset changes — from a footswitch, a knob, the buttons or a
MIDI controller — the whole display takes the colour of the new preset for a
moment, showing its number and name big:

![The display announcing preset 3](images/hept7-announce.png)

This works on every device, including the **MOD Duo**, which has no screen
for plugin pop-ups: it is how you see which preset a footswitch has just
selected.

## 2. The settings page

Click **Edit scenes** (or the gear icon) to open a page that takes the whole
screen and is made for touch: big cells, nothing smaller than a fingertip.

![The settings page](images/hept7-settings.png)

* **Scenes**: one row per preset. The round button on the left makes that
  preset the active one; the field next to it is its name; then one cell per
  CV output. A lit cell means that output is **on** while that preset is active.
  The row of the active preset is highlighted, and its lit cells are the CV
  signals actually leaving the plugin right now.
* **Cycle**: the last cell of each row, set apart from the CV outputs. On (↻,
  turquoise), the preset is one of those `Current preset` goes through when it
  is assigned to a footswitch, to the B+C pair or to a knob; off (✕), they skip
  it. See section 3.
* **Hardware**: tiles to assign a footswitch, a knob or a device screen page to
  the plugin's controls (section 3). A tile shows a check mark once it is assigned.
* **Saved presets**: the factory presets (section 5) and your own presets.

On a **phone** the page shows one preset at a time: pick it on the numbered
strip, edit its eight cells and its *Cycle* switch (the wide cell under them),
and use *Switch to preset N* to make it play.

![The settings page on a phone](images/hept7-settings-phone.png)

## 3. Switching presets with hardware

Assign one of the plugin's controls (the *Hardware* tiles, or the usual
assignment dialog) to the control you want to use:

| Control | Use |
|---|---|
| `Preset 1`–`7` | One footswitch per preset. Turning one on turns the others off. |
| `Current preset` | One control that goes through the presets: a footswitch (each press goes to the next preset), the B+C pair of footswitches (previous / next), or a knob. Also a CV or MIDI value. |

### Choosing which presets `Current preset` goes through

Seven presets may be more than a song needs. Turn off the **Cycle** cell of the
presets you do not use (settings page, section 2) and `Current preset` skips
them: with presets 1, 2 and 5 in the cycle, a footswitch assigned to it goes
1 → 2 → 5 → 1 → ..., and the B+C pair goes forwards and backwards through the
same three.

* The cycle always starts from the **active preset**. If a `Preset N`
  footswitch or a button of the plugin has just selected preset 5, the next
  press of `Current preset` goes to the preset after 5 in the cycle.
* Presets out of the cycle can still be selected: the plugin buttons and the
  `Preset 1`–`7` footswitches select any preset. Only `Current preset` skips them.
* With every preset in the cycle (the default), `Current preset` goes through
  all seven, also counting from the active preset.

> **How it works, and its limit.** The device keeps its own position for
> `Current preset` and sends the option after (or before) it on each press;
> nobody tells the device when the plugin changes the preset. The plugin
> therefore takes a move of one option as "next" (or "previous") and counts
> from the active preset, wherever the device's position is (a MIDI or CV
> value that moves it by exactly one option is a step too). So the value of
> `Current preset` (in the assignment and snapshot screens) may be the
> device's position instead of the active preset; the display, the buttons,
> the `Preset` footswitches and the name on the device screen always show the
> active preset. On a B+C pair that stops at the first and last options
> instead of going around, a press at an end does nothing until a press in the
> other direction.

## 4. The pop-up on the device screen

On devices with a screen that can show plugin pop-ups (MOD Dwarf), the name of
the active preset is shown in a pop-up whenever the preset changes. As with
Gossiper, the device only allows a pop-up on a page that has a control addressed
to the plugin, so:

1. Assign one of `Assign to notify 1`–`4` to a control of the device page where
   you want to see the pop-up (use a second, third or fourth one for other pages).
2. Every change of preset now shows the name of the preset on that page.

The footswitch LEDs and the preset knob follow the preset as well, with the
colour of each preset.

> The pop-up needs a firmware with the HMI feature. Without it (older firmware, or
> a MOD Duo) the plugin works the same, only without the pop-up and the LEDs: the
> display of the plugin is your feedback.

## 5. Factory presets

| Preset | Idea |
|---|---|
| **One-to-one selector** | Exactly one CV output is on at a time: preset N turns on output N (output 8 stays off). |
| **Seven-scene guitar rig** | Outputs 1 Compressor, 2 Overdrive, 3 Distortion, 4 Chorus, 5 Delay, 6 Reverb, 7 Boost, 8 Noise gate, and the presets *Clean, Chorus, Crunch, Lead, Solo, Ambient, Kill* (everything off). |
| **Layer builder** | Preset N turns on outputs 1 to N: stack one more layer on every press. |

## 6. Port reference

| Port | Type | Description |
|---|---|---|
| `CV Output 1`–`8` | CV output | 10 V while on in the active preset, 0 V otherwise. |
| `Preset 1`–`7` | Control input (toggle) | Footswitch selectors. |
| `Current preset` | Control input (enumeration) | Shows and selects the active preset; goes through the presets that are in the cycle (section 3). |
| `Assign to notify 1`–`4` | Control input | Assign to a device page so the pop-up shows there. No audio or CV effect. |
| `Preset N - Output M` | Control input (toggle) | Dip switch: is CV output M on while preset N is active? |
| `Preset N - In the cycle` | Control input (toggle) | Is preset N one of the presets `Current preset` goes through? On by default. |
| `Active preset` | Control output | The active preset (0 is preset 1), whatever selected it. Used by the interface of the plugin. |
| Preset label 1–7 | Plugin property (patch) | The name of each preset. Edited on the display or on the settings page. |

Names use letters, digits, spaces and `+ - : _ .`; anything else is replaced with `_`.
They are saved with the pedalboard and with the plugin presets.

## 7. Credits

Hept7 Switch is developed by Paulo Mateus (SrMouraSilva), see the
[plugins-lv2 repository](https://github.com/SrMouraSilva/plugins-lv2).
