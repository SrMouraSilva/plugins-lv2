// Copyright 2021-2026 Paulo Mateus
//
// SPDX-License-Identifier: Apache-2.0

// The pictures of the steps of the manuals: one object per picture, in plugins/resources/<plugin>.lv2/docs/images/<file>.
//
//   view       'shot' (the icon alone, on the dark of a pedalboard) or 'settings' (the settings page)
//   viewport   the browser window; a settings page is laid out for it (860 px wide: a tablet, with several pages)
//   setup      page expressions that put the plugin in the state the step is about (send({type: 'change', ...}), hostSelects(2))
//   highlight  selectors that get a ring (every match)
//   finger     the selector the finger points at (its lower-right part, so the element stays visible)
//   fingerAt   [x, y] as a fraction of that element, when the default would cover what matters (a text field)
//   crop       what the picture shows: a selector, or { closest: 'section', of: selector }
//   scrollTo   (settings) a selector: the page that holds it is brought into view first
//   query      more of the preview's query: jacks=1 keeps the jacks (the picture then includes them), device=390x844 is a phone
//   transparent  nothing behind the plugin (the dark of a pedalboard is the default); margin  pixels around the crop
//   scale      device pixel ratio (1.5 by default); wait  ms after setup before the picture (a toast stays about 2 s)
//   out        a path in the bundle instead of docs/images (modgui/screenshot.png), format 'png', width in pixels of the picture
//   crop 'window' is the whole window (a settings page as the user sees it)
//
// Run: node tools/media/shots.mjs all | <plugin>

const settings = (file, of, extra = {}) => ({
  file, view: 'settings', viewport: { width: 860, height: 540 }, scrollTo: of, highlight: [of], finger: of,
  crop: { closest: 'section', of }, ...extra,
});
const icon = (file, crop, extra = {}) => ({ file, view: 'shot', viewport: { width: 900, height: 640 }, crop, ...extra });
const tile = (symbol) => `[mod-port-symbol="${symbol}"]`;

// (The index of `preset=` in the query is the order of the preview's list, which is alphabetical.)
// The pictures of the cover and of the overviews: the plugin alone, nothing behind it, as it is (a settings page: the whole
// window, at the size of a laptop or of a phone). The states are the ones a reader finds when they open the plugin
const alone = (file, crop, extra = {}) => icon(file, crop, { transparent: true, margin: 2, scale: 1, ...extra });
// What MOD UI reads (they stay PNG): modgui/screenshot.png (500 px wide) and modgui/thumbnail.png (150 px), the plugin with
// its connectors (the sockets on both sides, as the other pedals' pictures have them) and nothing behind it: no plugs.
// The margin keeps the 2 px ring that the body casts (box-shadow) and the shadow of the sockets, which a tight crop cuts
const store = (crop, query) => ['screenshot', 'thumbnail'].map((name) => icon(`${name}.png`, crop, {
  out: `modgui/${name}.png`, format: 'png', width: name === 'screenshot' ? 500 : 150, transparent: true, margin: 4, query: `${query}&plugs=0`,
}));
const window_ = (file, width, height, extra = {}) => ({ file, view: 'settings', viewport: { width, height }, crop: 'window', scale: 1, ...extra });
// A phone: MOD UI has no viewport tag, so the page is laid out 1024 px wide and shrunk to the screen (see frame.html):
// the window is 1024 wide, and the picture is taken at 1.5 times the size of the phone's screen
const phone = (file, height, extra = {}) => window_(file, 1024, Math.round(height * 1024 / 390), { query: `device=390x${height}`, scale: 1.5 * 390 / 1024, ...extra });

// The display shows the control that moved last: the footswitch on the pedal, the knob in the settings page
const gossiper = (symbol, value) => `send({ type: 'change', symbol: '${symbol}', value: ${value} })`;
// Half Mouco: the custom pattern on (Pattern = Custom)
const HM_CUSTOM = `send({ type: 'change', symbol: 'pattern', value: 9 })`;
// Kill Switch: the custom pattern on (Pattern = Custom), with its three levels in use
const KS_CUSTOM = `send({ type: 'change', symbol: 'pattern', value: 10 })`;
const GOSSIPER_PEDAL = [gossiper('Potentiometer_1', 0.2), gossiper('Potentiometer_3', 0.63), gossiper('Footswitch_2', 1)];
const GOSSIPER_SETTINGS = [gossiper('Potentiometer_1', 0.2), gossiper('Footswitch_2', 1), gossiper('Potentiometer_3', 0.63)];

export const shots = {
  'tetr4-switch': [
    ...store('.tetr4-switch', 'jacks=1'),
    // The cover of the manual (2x) and the overview of the settings page, on a laptop and on a phone
    alone('panel.webp', '.tetr4-switch', { margin: 0, scale: 2 }),
    window_('settings.webp', 1440, 800, { query: 'addressed=Preset_selector_1' }),
    phone('settings-phone.webp', 740),
    // Quick start, step 4: Config 1 all down, Config 2 with switches 1 and 2 up
    icon('step-dip-switches.webp', '.tetr4-switch', {
      setup: [`send({type:'change',symbol:'Preset_1_output_1',value:0})`, `send({type:'change',symbol:'Preset_2_output_1',value:1})`],
      highlight: ['.t4-unit.p1 .t4-bank', '.t4-unit.p2 .t4-bank'], finger: '.t4-dip[data-preset="2"][data-output="1"]',
    }),
    // Step 5: the footswitches of the panel
    icon('step-footswitches.webp', '.tetr4-switch', {
      setup: [`send({type:'change',symbol:'Preset_1_output_1',value:0})`, `send({type:'change',symbol:'Preset_2_output_1',value:1})`, 'hostSelects(2)'],
      highlight: ['.t4-foot[data-preset="1"]', '.t4-foot[data-preset="2"]'], finger: '.t4-foot[data-preset="2"]',
    }),
    // The MIDI out panel: the channel is the first thing to set
    {
      file: 'step-midi.webp', view: 'settings', viewport: { width: 860, height: 540 }, scrollTo: '.t4s-midi-panel', crop: '.t4s-midi-panel',
      highlight: ['.t4s-midi-group[data-group="sending"] .t4s-field'], finger: '.t4s-num[data-symbol="MIDI_channel"] [data-dir="1"]',
    },
    // Step 6 and Option A: assign the presets to device footswitches
    settings('step-assign-preset-buttons.webp', tile('Preset_selector_1'), { highlight: [tile('Preset_selector_1'), tile('Preset_selector_2')] }),
    // Option B: one footswitch that cycles through the presets
    settings('step-assign-current-preset.webp', tile('Preset_combobox')),
    // Option C: the pop-up
    settings('step-assign-notify.webp', tile('Assign_to_notify_1')),
  ],

  'hept7-switch': [
    ...store('.hept7', 'jacks=1'),
    // "Seven-scene guitar rig" with preset 3 on: the display, the buttons and the jacks as the user sees them
    alone('hept7-pedalboard.webp', '.hept7', { query: 'jacks=1&preset=2', setup: ['hostSelects(3)'], wait: 2800 }),
    // ... and the display a moment after the change: it announces the preset (the toast stays for about two seconds)
    alone('hept7-announce.webp', '.hept7', { query: 'jacks=1&preset=2', setup: ['hostSelects(3)'], wait: 300 }),
    window_('hept7-settings.webp', 1366, 800, { query: 'preset=1' }),
    phone('hept7-settings-phone.webp', 844, { query: 'device=390x844&preset=1' }),
    settings('step-assign-preset-buttons.webp', tile('Preset_selector_1'), { highlight: [tile('Preset_selector_1'), tile('Preset_selector_2')] }),
    // Section 3: leave the presets you do not use out of the cycle of Current preset
    {
      file: 'step-cycle.webp', view: 'settings', viewport: { width: 1200, height: 760 },
      setup: [3, 4, 6, 7].map((n) => `send({type:'change',symbol:'Preset_${n}_in_cycle',value:0})`),
      highlight: ['.h7s-cell-cycle[data-preset="3"]', '.h7s-cell-cycle[data-preset="4"]', '.h7s-cell-cycle[data-preset="6"]', '.h7s-cell-cycle[data-preset="7"]'],
      finger: '.h7s-cell-cycle[data-preset="4"]', crop: { closest: '.h7s-panel', of: '.h7s-cell-cycle[data-preset="4"]' },
    },
    // Section 4: the pop-up
    settings('step-assign-notify.webp', tile('Assign_to_notify_1')),
  ],

  'kill-switch': [
    ...store('.kill-switch', 'jacks=1'),
    // The cover of the manual (2x)
    alone('panel.webp', '.kill-switch', { margin: 0, scale: 2, query: 'jacks=1' }),
    // Quick start, step 3: the arrows and the steps of the pattern
    icon('step-pattern.webp', '.kill-switch', {
      setup: ['send({type:"change",symbol:"pattern",value:6})', 'send({type:"change",symbol:"bpm",value:89})'],
      highlight: ['.ks-prev', '.ks-next', '.ks-steps'], finger: '.ks-next',
    }),
    // The settings page: the editor of the custom pattern, the controls to assign and the saved presets
    window_('settings.webp', 1366, 800, { setup: [KS_CUSTOM] }),
    phone('settings-phone.webp', 844, { setup: [KS_CUSTOM] }),
    // The custom pattern, Quick start of section 4: tapping a step goes to its next level
    settings('step-custom-pattern.webp', '.kss-steps', {
      setup: [KS_CUSTOM], highlight: ['.kss-cell:nth-child(3)'], finger: '.kss-cell:nth-child(3)', crop: { closest: 'section', of: '.kss-steps' },
    }),
    // Step 5: Depth and Edge
    icon('step-feel.webp', '.kill-switch', {
      setup: ['send({type:"change",symbol:"depth",value:60})', 'send({type:"change",symbol:"edge",value:6})'],
      highlight: ['.ks-depth', '.ks-edge'], finger: '.ks-edge-input', fingerAt: [0.8, 0.5],
    }),
  ],

  'half-mouco': [
    ...store('.half-mouco', 'jacks=1'),
    // The cover of the manual (2x)
    alone('panel.webp', '.half-mouco', { margin: 0, scale: 2, query: 'jacks=1' }),
    // Quick start, step 4: the arrows and the two rows
    icon('step-pattern.webp', '.half-mouco', {
      setup: ['send({type:"change",symbol:"pattern",value:7})', 'send({type:"change",symbol:"bpm",value:90})'],
      highlight: ['.hm-prev', '.hm-next', '.hm-lane'], finger: '.hm-next',
    }),
    // Step 6: the Endings tab, and the ending drawn along its time
    icon('step-endings.webp', '.half-mouco', {
      setup: ['send({type:"change",symbol:"mode",value:1})', 'send({type:"change",symbol:"ending",value:0})'],
      highlight: ['.hm-mode[data-mode="1"]', '.hm-lane'], finger: '.hm-mode[data-mode="1"]',
    }),
    // The helicopter, along its time (section 4)
    icon('ending-helicopter.webp', '.hm-display', {
      setup: ['send({type:"change",symbol:"mode",value:1})', 'send({type:"change",symbol:"ending",value:1})', `document.querySelector('.half-mouco').classList.remove('is-bypassed')`], margin: 6,
    }),
    // The settings page: the editor of the custom pattern, the controls to assign and the saved presets
    window_('settings.webp', 1366, 800, { setup: [HM_CUSTOM] }),
    phone('settings-phone.webp', 844, { setup: [HM_CUSTOM] }),
    settings('step-custom-pattern.webp', '.hms-steps', {
      setup: [HM_CUSTOM], highlight: ['.hms-steps[data-side="l"] .hms-cell:nth-child(3)', '.hms-steps[data-side="r"] .hms-cell:nth-child(3)'], finger: '.hms-steps[data-side="r"] .hms-cell:nth-child(3)', crop: { closest: 'section', of: '.hms-steps' },
    }),
  ],

  'gossiper': [
    ...store('.gossiper', 'jacks=1'),
    // Footswitch 2 is on, Pot 1 at 20% and Pot 3 at 63%: the display says which control moved last
    alone('gossiper-overview.webp', '.gossiper', { query: 'jacks=1', setup: GOSSIPER_PEDAL, wait: 1200 }),
    window_('plugin-settings.webp', 1366, 800, { setup: GOSSIPER_SETTINGS, wait: 1200 }),
    settings('settings-step1-assign-footswitch.webp', tile('Footswitch_1')),
    settings('settings-step2-assign-to-notify.webp', tile('Assign_to_notify_1')),
    // Renaming a footswitch, on the pedal
    icon('step-rename.webp', '.gossiper', {
      setup: [`(() => { const el = document.querySelector('.gp-label[data-n="2"]'); el.focus(); el.value = 'Lead'; $(el).trigger('input'); })()`],
      highlight: ['.gp-label[data-n="2"]'], finger: '.gp-label[data-n="2"]', fingerAt: [0.93, 0.8],
      query: 'jacks=1',
    }),
    // The range of a potentiometer
    settings('step-min-max.webp', '#gps-min-1', { highlight: ['#gps-min-1', '#gps-max-1'] }),
  ],
};
