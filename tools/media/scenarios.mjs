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
      // pages: scenes, Current / Next / Previous preset, screen pop-ups, MIDI out, saved presets
      await visitPages(ctx, {
        next: '.h7s-next', dot: '.h7s-dot', tile: true,
        tileSymbol: (page) => ['', 'Next_preset', 'Assign_to_notify_1'][page] || '',
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
    // The footswitch of the pedal is the Kill port: a tap on it is what the plugin sees (the preview does not simulate the
    // device's own controls). The pedal starts off, on Chop 8ths, and ends the same way
    demo: pedalboard('kill-switch', '.kill-switch', true, async (ctx) => {
      const send = (symbol, value) => ctx.eval(`send({ type: 'change', symbol: '${symbol}', value: ${value} })`);
      const bpm = () => ctx.eval(`Number(document.querySelector('.ks-bpm b').textContent)`);
      await ctx.sleep(700);
      await ctx.tap('.ks-foot');                                          // on: the chop starts with its first step
      await ctx.sleep(900);
      await ctx.tap('.ks-next');                                          // Chop 16ths
      await ctx.sleep(450);
      await ctx.tap('.ks-segment[data-speed="2"]');                       // 2x
      await ctx.sleep(450);
      await ctx.tap('.ks-bpm-up', 900);                                   // held: it goes on
      await ctx.sleep(400);
      for (let i = 0; i < 9; i++) await ctx.tap('.ks-next', 40);          // a pattern with a row for each side: Ping-pong 8ths
      await ctx.sleep(1200);
      await ctx.tap('.ks-mode[data-mode="1"]');                           // the endings
      await ctx.sleep(500);
      await ctx.tap('.ks-next');                                          // Helicopter
      await ctx.sleep(700);
      await ctx.tap('.ks-foot');                                          // off ...
      await ctx.sleep(500);
      await ctx.tap('.ks-foot');                                          // ... and on again: the ending plays from its start
      await ctx.sleep(1300);
      await ctx.tap('.ks-foot');                                          // off
      await ctx.tap('.ks-mode[data-mode="0"]');                           // back to the patterns
      await ctx.sleep(300);
      await ctx.tap('.ks-segment[data-speed="1"]');                       // 1x
      await send('pattern', 0); await send('ending', 0);                  // Chop 8ths, as a controller would put it
      await ctx.tap('.ks-bpm-down', 900);                                 // held: it goes back down
      while ((await bpm()) !== 120) await ctx.tap((await bpm()) > 120 ? '.ks-bpm-down' : '.ks-bpm-up', 60);
      await ctx.sleep(500);
    }),
    // What plays (a pattern, an ending), the LEDs that show it, the question that comes before a Custom pattern, the
    // steps going through their three levels, the length, and every page. The pages depend on the size of the window,
    // so the script finds where things are
    settings: settings('kill-switch', async (ctx) => {
      const send = (symbol, value) => ctx.eval(`send({ type: 'change', symbol: '${symbol}', value: ${value} })`);
      // The grid of patterns when it is there, the arrows when the screen is small
      const choose = async (index, from) => {
        if (await ctx.eval(`getComputedStyle(document.querySelector('.kss-choices')).display !== 'none'`)) {
          await ctx.tap(`.kss-choice[data-index="${index}"]`);
        } else {
          for (let i = from; i < index; i++) await ctx.tap('.kss-pick-next', 60);
        }
      };
      await ctx.sleep(800);
      await ctx.tap('[mod-port-symbol="kill"]', 120);                     // Kill assigned to a footswitch (marked as MOD UI does once its dialog is confirmed)
      await ctx.eval(`document.querySelector('[mod-port-symbol="kill"]').classList.add('addressed')`);
      await send('kill', 1);
      await ctx.sleep(700);
      await choose(12, 0);                                                // Triplet pong: a row for each side
      await ctx.sleep(900);
      await ctx.tap('.kss-cell[data-side="r"][data-step="1"]', 120);      // a built-in pattern: it asks first
      await ctx.sleep(1000);
      await ctx.tap('.kss-modal-ok', 120);                                // Create custom pattern
      await ctx.sleep(700);
      for (let i = 0; i < 3; i++) { await ctx.tap('.kss-cell[data-side="l"][data-step="0"]', 100); await ctx.sleep(450); }   // cut, half, pass, and cut again
      for (let i = 0; i < 2; i++) { await ctx.tap('.kss-cell[data-side="r"][data-step="3"]', 100); await ctx.sleep(450); }
      await ctx.tap('.kss-length-up', 100); await ctx.tap('.kss-length-up', 100);
      await ctx.tap('.kss-beat-up', 100);
      await ctx.sleep(700);
      // going through every page with the arrows, from the first one; the small button beside the tempo assigns it to
      // the device on the page that shows it
      let assigned = false;
      const assignTempo = async () => {
        if (assigned || !(await ctx.eval(`!!document.querySelector('[mod-port-symbol="bpm"]')?.offsetParent`))) return;
        assigned = true;
        await ctx.tap('[mod-port-symbol="bpm"]', 120);
        await ctx.eval(`document.querySelector('[mod-port-symbol="bpm"]').classList.add('addressed')`);
        await ctx.sleep(600);
      };
      await assignTempo();
      await visitPages(ctx, { next: '.kss-next', dot: '.kss-dot', tile: false, onPage: assignTempo });
      // Back to the start, as a controller would put it (Kill is off again, the first edit switched Pattern to Custom)
      await send('kill', 0);
      await send('pattern', 0);
      await send('custom_length', 8);
      await send('custom_steps_per_beat', 2);
      const left = [2, 1, 0, 1, 2, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2];
      const right = [0, 1, 2, 1, 0, 1, 2, 1, 2, 2, 2, 2, 2, 2, 2, 2];
      for (let i = 0; i < 16; i++) { await send(`custom_left_step_${i + 1}`, left[i]); await send(`custom_right_step_${i + 1}`, right[i]); }
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
