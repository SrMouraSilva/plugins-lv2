// Copyright 2021-2026 Paulo Mateus
//
// SPDX-License-Identifier: Apache-2.0

// Takes the pictures of the steps of the manuals (see shot-specs.mjs): the plugin in the state of the step, the target
// ringed, and the finger on it:
//
//   node tools/media/shots.mjs all           every picture of every plugin
//   node tools/media/shots.mjs gossiper      the pictures of one plugin
//
// The preview is started when it is not running already. Needs Node 22+ and Google Chrome.
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { launch, sleep } from './cdp.mjs';
import { shots } from './shot-specs.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const PORT = Number(process.env.PREVIEW_PORT || 8000);
const BASE = `http://localhost:${PORT}`;
const SCALE = 1.5;     // sharper than the screen: the pictures are shown in a column narrower than the window

const previewIsUp = () => fetch(`${BASE}/api/plugin/tetr4-switch`).then((r) => r.ok, () => false);

const STYLE = `(() => {
  const s = document.createElement('style');
  s.textContent = '.__ring { outline: 4px solid #ffc22e !important; outline-offset: 3px; border-radius: 12px; box-shadow: 0 0 0 8px rgba(0, 0, 0, .28) !important; }' +
    '#__finger { position: fixed; z-index: 2147483000; width: 44px; height: 44px; margin: -22px 0 0 -22px; border-radius: 50%; background: rgba(255,255,255,.6); box-shadow: 0 0 0 2px #fff, 0 2px 10px rgba(0,0,0,.45); pointer-events: none; transform: scale(.78); }';
  document.head.appendChild(s);
})()`;

async function take(plugin, spec) {
  const page = await launch(spec.viewport);
  await page.open(`${BASE}/frame.html?plugin=${plugin}&view=${spec.view}&latency=0${spec.query ? '&' + spec.query : ''}`);
  if (spec.view === 'shot') {
    await page.eval(`document.head.insertAdjacentHTML('beforeend', '<style>html, body, #pedalboard-dashboard { background: #14161d !important; }</style>')`);
  }
  await page.eval(STYLE);
  for (const expression of spec.setup || []) { await page.eval(expression); await sleep(150); }

  const rect = await page.eval(`(async () => {
    const q = (s) => document.querySelector(s);
    const target = ${JSON.stringify(spec.scrollTo || null)} && q(${JSON.stringify(spec.scrollTo || '')});
    if (target) {
      // the page of the pager that holds it: the ancestor whose parent scrolls sideways
      let page = target;
      while (page.parentElement && getComputedStyle(page.parentElement).overflowX !== 'auto') page = page.parentElement;
      if (page.parentElement) page.parentElement.scrollLeft = page.offsetLeft;
      await new Promise((r) => setTimeout(r, 700));
    }
    ${JSON.stringify(spec.highlight || [])}.forEach((s) => document.querySelectorAll(s).forEach((e) => e.classList.add('__ring')));
    const finger = ${JSON.stringify(spec.finger || null)} && q(${JSON.stringify(spec.finger || '')});
    if (finger) {
      const r = finger.getBoundingClientRect(), f = document.createElement('div');
      f.id = '__finger'; f.style.left = (r.left + r.width * ${(spec.fingerAt || [.72, .72])[0]}) + 'px'; f.style.top = (r.top + r.height * ${(spec.fingerAt || [.72, .72])[1]}) + 'px';
      document.body.appendChild(f);
    }
    const crop = ${JSON.stringify(spec.crop)};
    const el = typeof crop === 'string' ? q(crop) : q(crop.of).closest(crop.closest);
    const r = el.getBoundingClientRect(), m = ${spec.view === 'shot' ? 14 : 6};
    return { x: Math.max(0, r.left - m), y: Math.max(0, r.top - m), width: r.width + 2 * m, height: r.height + 2 * m };
  })()`);
  await sleep(400);
  const out = path.join(ROOT, 'plugins/resources', `${plugin}.lv2`, 'docs/images', spec.file);
  fs.writeFileSync(out, await page.image(rect, { format: 'webp', quality: 90, scale: SCALE }));
  await page.close();
  console.log(`${plugin}: ${spec.file}`);
}

const [which = 'all'] = process.argv.slice(2);
let server = null;
if (!(await previewIsUp())) {
  server = spawn('python3', [path.join(ROOT, 'tools/modgui-preview/serve.py'), '--port', String(PORT)], { stdio: 'ignore' });
  for (let i = 0; i < 25 && !(await previewIsUp()); i++) await sleep(200);
}
try {
  for (const [plugin, list] of Object.entries(shots)) {
    if (which !== 'all' && which !== plugin) continue;
    for (const spec of list) await take(plugin, spec);
  }
} finally {
  server?.kill();
}
process.exit(0);
