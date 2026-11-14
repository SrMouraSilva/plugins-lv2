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
//
// Run: node tools/media/shots.mjs all | <plugin>

const settings = (file, of, extra = {}) => ({
  file, view: 'settings', viewport: { width: 860, height: 540 }, scrollTo: of, highlight: [of], finger: of,
  crop: { closest: 'section', of }, ...extra,
});
const icon = (file, crop, extra = {}) => ({ file, view: 'shot', viewport: { width: 900, height: 640 }, crop, ...extra });
const tile = (symbol) => `[mod-port-symbol="${symbol}"]`;

export const shots = {
  'tetr4-switch': [
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
    // Step 6 and Option A: assign the presets to device footswitches
    settings('step-assign-preset-buttons.webp', tile('Preset_selector_1'), { highlight: [tile('Preset_selector_1'), tile('Preset_selector_2')] }),
    // Option B: one footswitch that cycles through the presets
    settings('step-assign-current-preset.webp', tile('Preset_combobox')),
    // Option C: the pop-up
    settings('step-assign-notify.webp', tile('Assign_to_notify_1')),
  ],

  'hept7-switch': [
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

  'gossiper': [
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
