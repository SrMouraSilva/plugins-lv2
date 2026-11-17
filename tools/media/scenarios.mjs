// Copyright 2021-2026 Paulo Mateus
//
// SPDX-License-Identifier: Apache-2.0

// What each GIF shows. A scenario is { viewport, query (of the preview frame), clip (a page expression that gives the
// rectangle to record), background, run(ctx), hold (ms the last frame stays) }. ctx: tap(selector), type(selector,
// text), eval(expression), sleep(ms). Keep them short: 10 to 20 seconds.

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

// The plugin alone, on the dark of a pedalboard, with a margin around it. With jacks the rectangle also holds them:
// a column of jacks rests on the outer edge of the plugin and its plugs stick out
const around = (selector, margin = 14, jacks = false) =>
  `(() => {
    let r = document.querySelector('${selector}').getBoundingClientRect();
    const j = ${jacks} && document.querySelector('.mod-pedal-output')?.getBoundingClientRect();
    if (j) r = { left: Math.min(r.left, j.left), top: Math.min(r.top, j.top), right: Math.max(r.right, j.right), bottom: Math.max(r.bottom, j.bottom) };
    return { x: r.left - ${margin}, y: r.top - ${margin}, width: r.right - r.left + ${margin * 2}, height: r.bottom - r.top + ${margin * 2} };
  })()`;

const pedalboard = (plugin, selector, jacks, run) => ({
  viewport: { width: 900, height: 640 },
  query: `view=shot&latency=150${jacks ? '&jacks=1' : ''}`,
  background: '#14161d',
  clip: around(selector, 14, jacks),
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
// onPage(page): something to do on a page once it is shown (the MIDI page of Tetr4 Switch: its fields are touched)
async function visitPages(ctx, { next, dot, tile, tileSymbol, onPage }) {
  const pages = await ctx.eval(`document.querySelectorAll('${dot}').length`);
  for (let page = 1; page < pages; page++) {
    await ctx.tap(next, 120);
    await ctx.sleep(900);
    if (tile && await ctx.eval(`!!document.querySelector('[mod-port-symbol="${tileSymbol(page)}"]')?.offsetParent`)) {
      await ctx.tap(`[mod-port-symbol="${tileSymbol(page)}"]`, 120);
      await ctx.eval(`document.querySelector('[mod-port-symbol="${tileSymbol(page)}"]').classList.add('addressed')`);
      await ctx.sleep(700);
    }
    if (onPage) await onPage(page);
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
      // the button beside the round one assigns the preset to a control of the device (marked as MOD UI does once its dialog is confirmed)
      await ctx.tap('[mod-port-symbol="Preset_selector_2"]', 120);
      await ctx.eval(`document.querySelector('[mod-port-symbol="Preset_selector_2"]').classList.add('addressed')`);
      await ctx.sleep(400);
      // pages: scenes, Current preset, screen pop-ups, MIDI out, saved presets
      await visitPages(ctx, {
        next: '.t4s-next', dot: '.t4s-dot', tile: true,
        tileSymbol: (page) => ['', 'Preset_combobox', 'Assign_to_notify_1'][page] || '',
        onPage: async (page) => {
          if (page !== 3) return;
          await ctx.tap('.t4s-num[data-symbol="MIDI_channel"] [data-dir="1"]', 120);
          await ctx.sleep(250);
          await ctx.tap('.t4s-num[data-symbol="MIDI_CC_1"] [data-dir="1"]', 120);
          await ctx.sleep(250);
          await ctx.tap('.t4s-switch', 120);
          await ctx.sleep(500);
        },
      });
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
      await ctx.sleep(500);
      // the button beside the number of the row assigns the preset to a control of the device (marked as MOD UI does once its dialog is confirmed)
      await ctx.tap('[mod-port-symbol="Preset_selector_3"]', 120);
      await ctx.eval(`document.querySelector('[mod-port-symbol="Preset_selector_3"]').classList.add('addressed')`);
      await ctx.sleep(400);
      // pages: scenes, Current preset, screen pop-ups, MIDI out, saved presets
      await visitPages(ctx, {
        next: '.h7s-next', dot: '.h7s-dot', tile: true,
        tileSymbol: (page) => ['', 'Preset_combobox', 'Assign_to_notify_1'][page] || '',
        onPage: async (page) => {
          if (page !== 3) return;
          await ctx.tap('.h7s-num[data-symbol="MIDI_channel"] [data-dir="1"]', 120);
          await ctx.sleep(250);
          await ctx.tap('.h7s-num[data-symbol="MIDI_CC_1"] [data-dir="1"]', 120);
          await ctx.sleep(250);
          await ctx.tap('.h7s-switch', 120);
          await ctx.sleep(500);
        },
      });
    }),
  },

  'kill-switch': {
    // The bypass footswitch is driven by MOD UI, not by the preview: a tap on it is followed by what MOD UI then tells the plugin
    demo: pedalboard('kill-switch', '.kill-switch', true, async (ctx) => {
      const bypass = (value) => ctx.eval(`send({ type: 'change', symbol: ':bypass', value: ${value} })`);
      const bpm = () => ctx.eval(`Number(document.querySelector('.ks-bpm b').textContent)`);
      await ctx.sleep(900);
      await ctx.tap('.ks-next');                                          // Chop 16ths
      await ctx.sleep(600);
      await ctx.tap('.ks-next');                                          // Triplet chop
      await ctx.sleep(600);
      await ctx.tap('.ks-segment[data-speed="2"]');                       // 2x
      await ctx.sleep(600);
      await ctx.tap('.ks-bpm-up', 1300);                                  // held: it goes on
      await ctx.sleep(600);
      await ctx.tap('.ks-foot'); await bypass(1);                         // bypassed
      await ctx.sleep(1300);
      await ctx.tap('.ks-foot'); await bypass(0);                         // on again
      await ctx.sleep(900);
      await ctx.tap('.ks-prev');
      await ctx.tap('.ks-prev');                                          // back to Chop 8ths
      await ctx.tap('.ks-segment[data-speed="1"]');                       // 1x
      await ctx.tap('.ks-bpm-down', 1300);                                // held: it goes back down
      while ((await bpm()) !== 120) await ctx.tap((await bpm()) > 120 ? '.ks-bpm-down' : '.ks-bpm-up', 60);
      await ctx.sleep(700);
    }),
    // What plays (a pattern, then Custom), its tempo and the editor of the custom pattern: a step goes through its three
    // levels in a circle, the length and the steps per beat. On a screen with room the editor sits under the choices; on a
    // small one a button takes to it
    settings: settings('kill-switch', async (ctx) => {
      await ctx.sleep(800);
      await ctx.tap('.kss-choice[data-index="5"]');                       // Gallop
      await ctx.sleep(900);
      await ctx.tap('.kss-choice[data-index="10"]');                      // Custom
      await ctx.sleep(800);
      if (await ctx.eval(`getComputedStyle(document.querySelector('.kss-edit-pattern')).display !== 'none'`)) {
        await ctx.tap('.kss-edit-pattern');                               // the editor is on a page of its own
        await ctx.sleep(1100);
      }
      for (let i = 0; i < 3; i++) { await ctx.tap('.kss-cell:nth-child(1)', 100); await ctx.sleep(450); }     // cut, half, pass, and cut again
      await ctx.tap('.kss-cell:nth-child(4)', 100);                       // pass -> cut
      await ctx.sleep(450);
      await ctx.tap('.kss-length-up', 100); await ctx.tap('.kss-length-up', 100);
      await ctx.tap('.kss-beat-up', 100);
      await ctx.sleep(700);
      await visitPages(ctx, { next: '.kss-next', dot: '.kss-dot', tile: true, tileSymbol: () => 'pattern' });
      // Back to the start, as a controller would put it (the first edit switched Pattern to Custom)
      for (const [symbol, value] of [['pattern', 0], ['custom_step_4', 2], ['custom_length', 8], ['custom_steps_per_beat', 2]]) {
        await ctx.eval(`send({ type: 'change', symbol: '${symbol}', value: ${value} })`);
      }
      await ctx.sleep(600);
    }, 540),
  },

  'half-mouco': {
    // The bypass footswitch is driven by MOD UI, not by the preview: a tap on it is followed by what MOD UI then tells the plugin
    demo: pedalboard('half-mouco', '.half-mouco', true, async (ctx) => {
      const send = (symbol, value) => ctx.eval(`send({ type: 'change', symbol: '${symbol}', value: ${value} })`);
      await ctx.sleep(900);
      await ctx.tap('.hm-next');                                          // Ping-pong 16ths
      await ctx.sleep(600);
      await ctx.tap('.hm-next');                                          // Triplet pong
      await ctx.sleep(600);
      await ctx.tap('.hm-segment[data-speed="2"]');                       // 2x
      await ctx.sleep(700);
      await ctx.tap('.hm-mode[data-mode="1"]');                           // the endings
      await ctx.sleep(900);
      await ctx.tap('.hm-next');                                          // Helicopter
      await ctx.sleep(900);
      await ctx.tap('.hm-length-up', 1100);                               // held: it goes on
      await ctx.sleep(900);
      await ctx.tap('.hm-foot'); await send(':bypass', 1);                // bypassed
      await ctx.sleep(1100);
      await ctx.tap('.hm-foot'); await send(':bypass', 0);                // on again
      await ctx.sleep(700);
      await ctx.tap('.hm-mode[data-mode="0"]');                           // back to the patterns
      await ctx.sleep(500);
      await ctx.tap('.hm-segment[data-speed="1"]');
      await ctx.tap('.hm-prev'); await ctx.tap('.hm-prev');               // Ping-pong 8ths
      // The ending and its length go back to the start, as a controller would put them
      await send('ending', 0); await send('length', 8);
      await ctx.sleep(700);
    }),
    // What plays (a pattern, an ending, Custom), its tempo and the editor of the custom pattern: a step goes through its
    // three levels in a circle, on each side. On a screen with room the editor sits under the choices; on a small one a
    // button takes to it
    settings: settings('half-mouco', async (ctx) => {
      const send = (symbol, value) => ctx.eval(`send({ type: 'change', symbol: '${symbol}', value: ${value} })`);
      await ctx.sleep(800);
      await ctx.tap('.hms-choice[data-index="5"]');                        // Gallop sides
      await ctx.sleep(900);
      await ctx.tap('.hms-mode[data-mode="1"]');                           // the endings
      await ctx.sleep(500);
      await ctx.tap('.hms-choice[data-index="1"]');                        // Helicopter
      await ctx.sleep(1000);
      await ctx.tap('.hms-mode[data-mode="0"]');                           // back to the patterns
      await ctx.sleep(500);
      await ctx.tap('.hms-choice[data-index="9"]');                        // Custom
      await ctx.sleep(800);
      if (await ctx.eval(`getComputedStyle(document.querySelector('.hms-edit-pattern')).display !== 'none'`)) {
        await ctx.tap('.hms-edit-pattern');                                // the editor is on a page of its own
        await ctx.sleep(1100);
      }
      for (let i = 0; i < 3; i++) { await ctx.tap('.hms-steps[data-side="l"] .hms-cell:nth-child(1)', 100); await ctx.sleep(450); }   // cut, half, pass, and cut again
      for (let i = 0; i < 3; i++) { await ctx.tap('.hms-steps[data-side="r"] .hms-cell:nth-child(1)', 100); await ctx.sleep(450); }
      await ctx.tap('.hms-length-up', 100); await ctx.tap('.hms-length-up', 100);
      await ctx.tap('.hms-beat-up', 100);
      await ctx.sleep(700);
      await visitPages(ctx, { next: '.hms-next', dot: '.hms-dot', tile: true, tileSymbol: () => 'mode' });
      // Back to the start, as a controller would put it
      for (const [symbol, value] of [['mode', 0], ['ending', 0], ['pattern', 0], ['custom_length', 8], ['custom_steps_per_beat', 2]]) {
        await send(symbol, value);
      }
      await ctx.sleep(600);
    }, 540),
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
