// Copyright 2021-2026 Paulo Mateus
//
// SPDX-License-Identifier: Apache-2.0

// A tiny Chrome DevTools driver (no dependencies): needs Node 22+ (global WebSocket) and Google Chrome.
// Set CHROME_BIN when Chrome is not at the usual place.
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const CHROME = [process.env.CHROME_BIN, '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', 'google-chrome', 'chromium']
  .filter(Boolean).find((bin) => !bin.includes('/') || fs.existsSync(bin));

export const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

export async function launch({ width, height, scale = 1 }) {
  const port = 9300 + Math.floor(Math.random() * 600);
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'media-chrome-'));
  const proc = spawn(CHROME, ['--headless=new', '--disable-gpu', '--hide-scrollbars', `--remote-debugging-port=${port}`,
    `--window-size=${width},${height}`, `--user-data-dir=${profile}`, 'about:blank'], { stdio: 'ignore' });

  let targets;
  for (let i = 0; i < 50 && !targets?.length; i++) {
    try { targets = await (await fetch(`http://127.0.0.1:${port}/json`)).json(); } catch { await sleep(200); }
  }
  const ws = new WebSocket(targets.find((t) => t.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve) => { ws.onopen = resolve; });

  let id = 0;
  const pending = new Map();
  ws.onmessage = (message) => {
    const data = JSON.parse(message.data);
    if (data.id && pending.has(data.id)) { pending.get(data.id)(data); pending.delete(data.id); }
  };
  const send = (method, params = {}) => new Promise((resolve) => { const i = ++id; pending.set(i, resolve); ws.send(JSON.stringify({ id: i, method, params })); });

  await send('Page.enable');
  await send('Emulation.setDeviceMetricsOverride', { width, height, deviceScaleFactor: scale, mobile: false });

  const page = {
    send,
    async open(url) {
      await send('Page.navigate', { url });
      for (let i = 0; i < 50; i++) { if (await page.eval('window.ready === true')) break; await sleep(200); }
      await sleep(400);
    },
    async eval(expression) {
      const r = await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
      if (r.result.exceptionDetails) throw new Error(JSON.stringify(r.result.exceptionDetails));
      return r.result.result.value;
    },
    // format: 'png' (the frames of a GIF) or 'webp' (the pictures of the documentation: lighter, and it keeps transparency)
    async image(clip, { format = 'png', quality = 92, scale = 1 } = {}) {
      const r = await send('Page.captureScreenshot', { format, ...(format === 'webp' ? { quality } : {}), ...(clip ? { clip: { ...clip, scale } } : {}) });
      return Buffer.from(r.result.data, 'base64');
    },
    png(clip) { return page.image(clip); },
    async close() {
      ws.close();
      proc.kill();
      await sleep(600);
      try { fs.rmSync(profile, { recursive: true, force: true, maxRetries: 5 }); } catch { /* Chrome is still closing: the system cleans the temp folder */ }
    },
  };
  return page;
}
