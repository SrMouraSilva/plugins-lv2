// Copyright 2021-2026 Paulo Mateus
//
// SPDX-License-Identifier: Apache-2.0

// What each GIF shows. A scenario is { viewport, query (of the preview frame), clip (a page expression that gives the
// rectangle to record), background, run(ctx), hold (ms the last frame stays) }. ctx: tap(selector), type(selector,
// text), eval(expression), sleep(ms). Keep them short: 10 to 20 seconds.

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

// The plugin alone, on the dark of a pedalboard, with a margin around it
const around = (selector, margin = 14) =>
  `(() => { const r = document.querySelector('${selector}').getBoundingClientRect(); return { x: r.left - ${margin}, y: r.top - ${margin}, width: r.width + ${margin * 2}, height: r.height + ${margin * 2} }; })()`;

const pedalboard = (plugin, selector, jacks, run) => ({
  viewport: { width: 900, height: 640 },
  query: `view=shot&latency=150${jacks ? '&jacks=1' : ''}`,
  background: '#14161d',
  clip: around(selector),
  run,
});

// The settings page, as a tablet shows it: it does not fit on one page, so the arrows of the top bar are used.
// next: the selector of the arrow; address: what MOD UI does once the assignment dialog is confirmed
const settings = (plugin, run, height = 540) => ({
  viewport: { width: 860, height },
  query: 'view=settings&latency=150',
  run,
});

// Goes through every page with the arrow of the bar, tapping one assignment tile on the way (it is marked as assigned,
// which is what MOD UI does when the dialog of the tile is confirmed), and comes back to the first page with its dot
async function visitPages(ctx, { next, dot, tile, tileSymbol }) {
  const pages = await ctx.eval(`document.querySelectorAll('${dot}').length`);
  for (let page = 1; page < pages; page++) {
    await ctx.tap(next, 120);
    await ctx.sleep(900);
    if (tile && await ctx.eval(`!!document.querySelector('[mod-port-symbol="${tileSymbol(page)}"]')?.offsetParent`)) {
      await ctx.tap(`[mod-port-symbol="${tileSymbol(page)}"]`, 120);
      await ctx.eval(`document.querySelector('[mod-port-symbol="${tileSymbol(page)}"]').classList.add('addressed')`);
      await ctx.sleep(700);
    }
  }
  await ctx.sleep(500);
  await ctx.tap(`${dot}[data-page="0"]`, 120);
  await ctx.sleep(900);
}

export const scenarios = {
  'tetr4-switch': {
    demo: pedalboard('tetr4-switch', '.tetr4-switch', false, async (ctx) => {
      await ctx.sleep(600);
      await ctx.tap('.t4-dip[data-preset="1"][data-output="2"]');        // preset 1: outputs 1 and 2
      await ctx.sleep(500);
      await ctx.tap('.t4-foot[data-preset="2"]');                         // a footswitch stomp: preset 2
      await ctx.sleep(700);
      await ctx.tap('.t4-dip[data-preset="2"][data-output="1"]');
      await ctx.tap('.t4-dip[data-preset="2"][data-output="4"]');
      await ctx.sleep(500);
      await ctx.type('.t4-name[data-preset="2"]', 'Lead');
      await ctx.tap('.t4-foot[data-preset="3"]');
      await ctx.sleep(800);
      await ctx.tap('.t4-foot[data-preset="4"]');
      await ctx.sleep(800);
      await ctx.tap('.t4-foot[data-preset="2"]');
      await ctx.sleep(900);
      await ctx.tap('.t4-foot[data-preset="1"]');
    }),
    settings: settings('tetr4-switch', async (ctx) => {
      await ctx.sleep(600);
      await ctx.tap('.t4s-dip[data-preset="1"][data-output="3"]');
      await ctx.tap('.t4s-pad[data-preset="2"]');
      await ctx.sleep(500);
      await ctx.type('.t4s-label[data-preset="2"]', 'Lead');
      await ctx.tap('.t4s-dip[data-preset="2"][data-output="1"]');
      await ctx.tap('.t4s-pad[data-preset="3"]');
      await ctx.sleep(700);
      await visitPages(ctx, { next: '.t4s-next', dot: '.t4s-dot', tile: true, tileSymbol: (page) => ['', 'Preset_selector_1', 'Preset_combobox', 'Assign_to_notify_1'][page] || '' });
    }, 360),
  },

  'hept7-switch': {
    demo: pedalboard('hept7-switch', '.hept7', true, async (ctx) => {
      await ctx.sleep(600);
      await ctx.tap('.h7-pad.p3');                                        // the display announces the preset
      await ctx.sleep(2000);
      await ctx.tap('.h7-out[data-output="4"]');
      await ctx.tap('.h7-out[data-output="6"]');
      await ctx.sleep(500);
      await ctx.type('.h7-label', 'Solo');
      await ctx.tap('.h7-pad.p5');
      await ctx.sleep(2000);
      await ctx.tap('.h7-pad.p2');
      await ctx.sleep(2000);
      await ctx.tap('.h7-pad.p7');
      await ctx.sleep(1500);
    }),
    settings: settings('hept7-switch', async (ctx) => {
      await ctx.sleep(600);
      await ctx.tap('.h7s-cell[data-preset="3"][data-output="2"]');
      await ctx.tap('.h7s-rowpad[data-preset="3"]');
      await ctx.sleep(500);
      await ctx.type('.h7s-label[data-preset="3"]', 'Crunch');
      await ctx.tap('.h7s-cell-cycle[data-preset="2"]');
      await ctx.sleep(700);
      await visitPages(ctx, { next: '.h7s-next', dot: '.h7s-dot', tile: true, tileSymbol: (page) => ['', '', '', 'Preset_selector_1', 'Preset_combobox', 'Assign_to_notify_1'][page] || '' });
    }),
  },

  'gossiper': {
    // Gossiper only shows what a controller did: the controls are moved from outside, as MIDI would
    demo: pedalboard('gossiper', '.gossiper', true, async (ctx) => {
      const send = (symbol, value) => ctx.eval(`send({ type: 'change', symbol: '${symbol}', value: ${value} })`);
      await ctx.sleep(700);
      await send('Footswitch_2', 1);
      await ctx.sleep(1200);
      for (const value of [0.1, 0.25, 0.4, 0.55, 0.63]) { await send('Potentiometer_3', value); await ctx.sleep(260); }
      await ctx.sleep(900);
      await ctx.type('.gp-label[data-n="5"]', 'Reverb');
      await send('Footswitch_5', 1);
      await ctx.sleep(1200);
      await send('Footswitch_2', 0);
      await ctx.sleep(1100);
      for (const value of [0.08, 0.15, 0.2]) { await send('Potentiometer_1', value); await ctx.sleep(260); }
      await ctx.sleep(900);
      await send('Footswitch_7', 1);
      await ctx.sleep(1300);
    }),
    settings: settings('gossiper', async (ctx) => {
      await ctx.sleep(600);
      await ctx.type('.gps-label[data-n="2"]', 'Lead');
      await ctx.sleep(500);
      await visitPages(ctx, { next: '.gps-next', dot: '.gps-dot', tile: true, tileSymbol: (page) => ['', 'Footswitch_1', 'Potentiometer_1', 'Assign_to_notify_1'][page] || '' });
    }),
  },
};
