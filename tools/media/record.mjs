// Copyright 2021-2026 Paulo Mateus
//
// SPDX-License-Identifier: Apache-2.0

// Records the GIFs of the Readme and of the manuals from the modgui preview (tools/modgui-preview):
//
//   node tools/media/record.mjs all                    every GIF of every plugin
//   node tools/media/record.mjs tetr4-switch demo      one GIF (kind: demo = the plugin on the pedalboard, settings = its settings page)
//
// The preview is started when it is not running already. Frames are captured while a script taps the interface (a
// circle shows the finger), and tools/media/build_gif.py puts them together into
// plugins/resources/<plugin>.lv2/docs/images/<plugin>-<kind>.gif. Needs Node 22+, Google Chrome and Pillow.
import { spawn, spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { launch, sleep } from './cdp.mjs';
import { scenarios } from './scenarios.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const PORT = Number(process.env.PREVIEW_PORT || 8000);
const BASE = `http://localhost:${PORT}`;

const previewIsUp = () => fetch(`${BASE}/api/plugin/tetr4-switch`).then((r) => r.ok, () => false);

async function ensurePreview() {
  if (await previewIsUp()) return null;
  const args = [path.join(ROOT, 'tools/modgui-preview/serve.py'), '--port', String(PORT), ...(process.env.MOD_UI_HTML ? ['--mod-ui', process.env.MOD_UI_HTML] : [])];
  const server = spawn('python3', args, { stdio: 'ignore' });
  for (let i = 0; i < 25 && !(await previewIsUp()); i++) await sleep(200);
  return server;
}

// The finger: a circle that glides to what is tapped, shrinks while it is pressed and fades when released
const FINGER = `(() => {
  const style = document.createElement('style');
  style.textContent = '#__finger { position: fixed; z-index: 2147483000; width: 44px; height: 44px; margin: -22px 0 0 -22px; border-radius: 50%;' +
    ' background: rgba(255,255,255,.35); box-shadow: 0 0 0 2px rgba(255,255,255,.9), 0 2px 10px rgba(0,0,0,.45); pointer-events: none; opacity: 0;' +
    ' transform: scale(1); transition: left .3s ease, top .3s ease, opacity .25s, transform .12s; }' +
    '#__finger.down { opacity: 1; transform: scale(.78); background: rgba(255,255,255,.6); } #__finger.shown { opacity: .75; }';
  document.head.appendChild(style);
  const el = document.createElement('div'); el.id = '__finger'; document.body.appendChild(el);
})()`;

async function record(plugin, kind) {
  const spec = scenarios[plugin]?.[kind];
  if (!spec) throw new Error(`no scenario for ${plugin} ${kind}`);

  const dir = fs.mkdtempSync(path.join(os.tmpdir(), `frames-${plugin}-${kind}-`));
  const page = await launch(spec.viewport);
  await page.open(`${BASE}/frame.html?plugin=${plugin}&${spec.query}`);
  if (spec.background) {
    await page.eval(`document.head.insertAdjacentHTML('beforeend', '<style>html, body, #pedalboard-dashboard { background: ${spec.background} !important; }</style>')`);
  }
  await page.eval(FINGER);

  // Capture frames in the background for as long as the script runs; each one lasts until the next one is taken
  const frames = [];
  let recording = true;
  const clip = spec.clip ? await page.eval(spec.clip) : null;
  const capture = (async () => {
    while (recording) {
      const png = await page.png(clip);
      frames.push({ at: Date.now(), png });
    }
  })();

  const finger = {
    async at(selector) {
      return page.eval(`(() => { const r = document.querySelector(${JSON.stringify(selector)}).getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; })()`);
    },
    async move(x, y, down) {
      await page.eval(`(() => { const f = document.getElementById('__finger'); f.style.left = '${x}px'; f.style.top = '${y}px'; f.classList.toggle('shown', true); f.classList.toggle('down', ${down}); })()`);
    },
    async release() { await page.eval(`document.getElementById('__finger').classList.remove('down')`); },
  };

  const mouse = (type, x, y) => page.send('Input.dispatchMouseEvent', { type, x, y, button: 'left', clickCount: 1 });
  const ctx = {
    page,
    sleep,
    eval: (expression) => page.eval(expression),
    // Taps the first element that matches: the finger glides there, presses, and lets go
    async tap(selector, hold = 140) {
      const { x, y } = await finger.at(selector);
      await finger.move(x, y, false);
      await sleep(380);
      await finger.move(x, y, true);
      await mouse('mouseMoved', x, y);
      await mouse('mousePressed', x, y);
      await sleep(hold);
      await mouse('mouseReleased', x, y);
      await finger.release();
      await sleep(180);
    },
    // Types into a text field, one letter at a time (the field is tapped first, and its text selected)
    async type(selector, text) {
      await ctx.tap(selector);
      await page.eval(`document.querySelector(${JSON.stringify(selector)}).select()`);
      await sleep(250);
      for (const letter of text) {
        await page.send('Input.insertText', { text: letter });
        await sleep(130);
      }
      await sleep(300);
    },
  };

  await sleep(500);
  await spec.run(ctx);
  await sleep(spec.hold ?? 900);
  recording = false;
  await capture;
  await page.close();

  // frames/NNNN.png and durations.json (ms) for build_gif.py
  frames.forEach((frame, i) => fs.writeFileSync(path.join(dir, `${String(i).padStart(4, '0')}.png`), frame.png));
  const durations = frames.map((frame, i) => (i + 1 < frames.length ? frames[i + 1].at - frame.at : spec.hold ?? 900));
  fs.writeFileSync(path.join(dir, 'durations.json'), JSON.stringify(durations));

  const out = path.join(ROOT, 'plugins/resources', `${plugin}.lv2`, 'docs/images', `${plugin}-${kind}.gif`);
  fs.mkdirSync(path.dirname(out), { recursive: true });
  const built = spawnSync('python3', [path.join(ROOT, 'tools/media/build_gif.py'), dir, out], { stdio: 'inherit' });
  fs.rmSync(dir, { recursive: true, force: true });
  if (built.status !== 0) throw new Error('build_gif.py failed');
}

const [plugin, kind] = process.argv.slice(2);
const server = await ensurePreview();
try {
  const jobs = plugin === 'all' || !plugin
    ? Object.entries(scenarios).flatMap(([name, kinds]) => Object.keys(kinds).map((k) => [name, k]))
    : [[plugin, kind || 'demo']];
  for (const [name, k] of jobs) {
    console.log(`recording ${name} ${k}`);
    await record(name, k);
  }
} finally {
  server?.kill();
}
process.exit(0);
