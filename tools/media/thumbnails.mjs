// Copyright 2021-2026 Paulo Mateus
//
// SPDX-License-Identifier: Apache-2.0

// Renders plugins/resources/<plugin>.lv2/docs/alternative-thumbnail.html to alternative-thumbnail.webp (400x400, transparent
// around the rounded square): node tools/media/thumbnails.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { launch, sleep } from './cdp.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const SIZE = 400;

// Every bundle that has docs/alternative-thumbnail.html
const plugins = fs.readdirSync(path.join(ROOT, 'plugins/resources'))
  .filter((name) => name.endsWith('.lv2') && fs.existsSync(path.join(ROOT, 'plugins/resources', name, 'docs/alternative-thumbnail.html')))
  .map((name) => name.slice(0, -4));

for (const plugin of plugins) {
  const docs = path.join(ROOT, 'plugins/resources', `${plugin}.lv2`, 'docs');
  const page = await launch({ width: SIZE, height: SIZE });
  await page.send('Emulation.setDefaultBackgroundColorOverride', { color: { r: 0, g: 0, b: 0, a: 0 } });
  await page.send('Page.navigate', { url: `file://${path.join(docs, 'alternative-thumbnail.html')}` });
  await sleep(800);
  fs.writeFileSync(path.join(docs, 'alternative-thumbnail.webp'), await page.image({ x: 0, y: 0, width: SIZE, height: SIZE }, { format: 'webp', quality: 96 }));
  await page.close();
  console.log(`${plugin}: alternative-thumbnail.webp`);
}
process.exit(0);
