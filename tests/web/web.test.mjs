// SPDX-License-Identifier: MIT
// Browser tests of the web build (Playwright + Chromium, run with `node --test tests/web`).
//
// Environment:
//   ZB_WEB_ST_DIR  build/web-st/bin (single-threaded build, page, loader, staged assets)
//   ZB_WEB_MT_DIR  build/web/bin (threaded build; the isolation test is skipped without it)
//   ZB_NATIVE_BIN  a native zauberer binary to host co-op sessions (default build/dev/bin;
//                  the co-op test is skipped without one)
import { test, before, after } from 'node:test';
import { spawn } from 'node:child_process';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import crypto from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startServer } from './server.mjs';

const require = createRequire(import.meta.url);
const { chromium, devices } = require('playwright');

const HERE = path.dirname(fileURLToPath(import.meta.url));
const REPO = path.resolve(HERE, '..', '..');
const ST_DIR = process.env.ZB_WEB_ST_DIR || path.join(REPO, 'build', 'web-st', 'bin');
const MT_DIR = process.env.ZB_WEB_MT_DIR || path.join(REPO, 'build', 'web', 'bin');
const HAS_MT = fs.existsSync(path.join(MT_DIR, 'zauberer.wasm'));
const NATIVE_BIN = process.env.ZB_NATIVE_BIN || path.join(REPO, 'build', 'dev', 'bin', 'zauberer');
const HAS_NATIVE = fs.existsSync(NATIVE_BIN) || fs.existsSync(`${NATIVE_BIN}.exe`);
const LAUNCH = { args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--autoplay-policy=user-gesture-required'] };

let browser;

before(async () => {
  assert.ok(fs.existsSync(path.join(ST_DIR, 'zauberer-st.wasm')), `no web-st build in ${ST_DIR}`);
  browser = await chromium.launch(LAUNCH);
});

after(async () => {
  await browser?.close();
});

/** A fresh copy of the web build (both variants) plus the test files. */
function makeDist() {
  const dist = fs.mkdtempSync(path.join(os.tmpdir(), 'zb-web-'));
  fs.cpSync(ST_DIR, dist, { recursive: true });
  if (HAS_MT) {
    for (const name of fs.readdirSync(MT_DIR)) {
      if (/^zauberer\.(js|wasm)$/.test(name)) fs.copyFileSync(path.join(MT_DIR, name), path.join(dist, name));
    }
  }
  fs.cpSync(path.join(REPO, 'tests', 'scenarios'), path.join(dist, 'tests', 'scenarios'), { recursive: true });
  fs.cpSync(path.join(REPO, 'tests', 'golden'), path.join(dist, 'tests', 'golden'), { recursive: true });
  return dist;
}

/** Opens the game; returns { page, server, logs, errors, close }. Pass a
 * server to revisit the same origin (Cache Storage and IndexedDB are per origin). */
async function open({ query = '', isolated = false, context = null, dist = null, device = null, server = null } = {}) {
  const root = server ? server.root : (dist || makeDist());
  const ownServer = !server;
  server = server || await startServer(root, { isolated });
  const ctx = context || await browser.newContext(device ? { ...devices[device] } : {});
  const page = await ctx.newPage();
  const logs = [];
  const errors = [];
  page.on('console', (m) => {
    logs.push(m.text());
    if (m.type() === 'error') errors.push(m.text());
  });
  page.on('pageerror', (e) => errors.push(String(e)));
  await page.goto(server.url + (query ? `?${query}` : ''));
  return {
    page, server, logs, errors, root,
    state: () => page.evaluate(() => (window.Module && Module.calledRun ? Module.zbState() : null)),
    close: async () => {
      if (!context) await ctx.close();
      else await page.close();
      if (ownServer) await server.close();
      if (!dist && ownServer) fs.rmSync(root, { recursive: true, force: true });
    },
  };
}

async function waitFor(fn, what, timeout = 60000) {
  const end = Date.now() + timeout;
  let last;
  while (Date.now() < end) {
    last = await fn();
    if (last) return last;
    await new Promise((r) => setTimeout(r, 100));
  }
  throw new Error(`timed out waiting for ${what} (last: ${JSON.stringify(last)})`);
}

async function waitForFrames(g, frames = 30) {
  return waitFor(async () => {
    const s = await g.state();
    return s && s.frames >= frames && s.ticks > 0 ? s : null;
  }, `${frames} frames`);
}

async function waitForLog(g, pattern, timeout = 60000) {
  return waitFor(async () => g.logs.find((l) => pattern.test(l)), String(pattern), timeout);
}

function assetRequests(requests) {
  return requests.filter((r) => r.startsWith('assets/') && r !== 'assets/index.tsv');
}

// spec: platform-targets: Static host without isolation headers
// spec: platform-targets: Each preset produces a runnable artifact
test('desktop page without isolation runs the single-threaded build', async () => {
  const g = await open();
  try {
    const s = await waitForFrames(g);
    assert.equal(await g.page.evaluate(() => window.zbBuild), 'zauberer-st');
    assert.equal(s.input_mode, 'desktop');
    assert.equal(s.script_errors, 0);
    assert.deepEqual(g.errors, []);
  } finally {
    await g.close();
  }
});

// spec: platform-targets: Isolated host
test('cross-origin isolated page runs the threaded build', { skip: !HAS_MT && 'no threaded web build' }, async () => {
  const g = await open({ isolated: true });
  try {
    await waitForFrames(g);
    assert.equal(await g.page.evaluate(() => self.crossOriginIsolated), true);
    assert.equal(await g.page.evaluate(() => window.zbBuild), 'zauberer');
    assert.deepEqual(g.errors, []);
  } finally {
    await g.close();
  }
});

// spec: test-automation: Mobile emulation
// spec: input-actions: Phone starts in touch mode
// spec: input-actions: Synthesised mouse events
// spec: input-actions: Keyboard on a tablet
test('phone emulation starts in touch mode', async () => {
  const g = await open({ device: 'Pixel 7' });
  try {
    const s = await waitForFrames(g);
    assert.equal(s.input_mode, 'touch');
    assert.deepEqual(g.errors, []);
    // A tap also produces synthesised mouse events: the mode stays touch.
    const box = await g.page.locator('#canvas').boundingBox();
    await g.page.touchscreen.tap(box.x + box.width / 2, box.y + box.height / 2);
    await g.page.waitForTimeout(300);
    assert.equal((await g.state()).input_mode, 'touch');
    // A physical key switches to desktop mode.
    await g.page.keyboard.press('KeyC');
    await waitFor(async () => (await g.state()).input_mode === 'desktop', 'desktop mode after a key');
  } finally {
    await g.close();
  }
});

// spec: asset-packages: Second visit uses the cache
test('second visit loads assets from Cache Storage', async () => {
  const dist = makeDist();
  const server = await startServer(dist);
  const context = await browser.newContext();
  try {
    const first = await open({ server, context });
    await waitForFrames(first);
    await waitFor(async () => first.page.evaluate(() => Module.zbStats.network.includes('base/models/carpet.glb')),
      'the carpet model to be fetched');
    assert.ok(assetRequests(server.requests).length >= 10, 'first visit downloads assets');
    await first.close();
    server.requests.length = 0;
    const second = await open({ server, context });
    await waitForFrames(second);
    await second.page.waitForTimeout(500);
    assert.deepEqual(assetRequests(server.requests), [], 'no asset is downloaded again');
    assert.ok(await second.page.evaluate(() => Module.zbStats.cache.length) >= 10);
    await second.close();
  } finally {
    await context.close();
    await server.close();
    fs.rmSync(dist, { recursive: true, force: true });
  }
});

// spec: asset-packages: Updated asset
test('an asset whose hash changed is downloaded again', async () => {
  const dist = makeDist();
  const server = await startServer(dist);
  const context = await browser.newContext();
  try {
    const first = await open({ server, context });
    await waitForFrames(first);
    await first.close();
    // Publish a new version of one boot file and update its index entry.
    const rel = 'base/data/third_party.lua';
    const file = path.join(dist, 'assets', rel);
    const content = fs.readFileSync(file, 'utf8') + '-- updated\n';
    fs.writeFileSync(file, content);
    const sha = crypto.createHash('sha256').update(content).digest('hex');
    const indexFile = path.join(dist, 'assets', 'index.tsv');
    const index = fs.readFileSync(indexFile, 'utf8').split('\n').map((line) =>
      line.startsWith(rel + '\t') ? [rel, Buffer.byteLength(content), sha, '1'].join('\t') : line);
    fs.writeFileSync(indexFile, index.join('\n'));
    server.requests.length = 0;
    const second = await open({ server, context });
    await waitForFrames(second);
    assert.deepEqual(assetRequests(server.requests), [`assets/${rel}`]);
    const loaded = await second.page.evaluate((p) => Module.FS.readFile(p, { encoding: 'utf8' }), `assets/${rel}`);
    assert.equal(loaded, content, 'the new version is used');
    await second.close();
  } finally {
    await context.close();
    await server.close();
    fs.rmSync(dist, { recursive: true, force: true });
  }
});

// spec: audio-playback: Music requested before interaction
test('music starts at the first user gesture', async () => {
  const g = await open();
  try {
    let s = await waitForFrames(g);
    assert.equal(s.audio_locked, true);
    assert.equal(s.audio_device, false);
    assert.ok(s.voices >= 1, 'the music voice waits for the gesture');
    await g.page.mouse.click(200, 200);
    s = await waitFor(async () => {
      const st = await g.state();
      return st && !st.audio_locked && st.audio_device ? st : null;
    }, 'audio to unlock');
    assert.ok(s.voices >= 1, 'the music plays');
    assert.deepEqual(g.errors, []);
  } finally {
    await g.close();
  }
});

// spec: platform-targets: Data persists across runs
test('save data persists in IndexedDB across page loads', async () => {
  const dist = makeDist();
  const server = await startServer(dist);
  const context = await browser.newContext();
  try {
    const write = await open({ server, context, query: 'args=--scenario%20tests/scenarios/save_write.lua&files=tests/scenarios/save_write.lua' });
    await waitForLog(write, /scenario 'save write': passed/);
    await waitFor(async () => write.page.evaluate(() => Module.zbStats.synced > 0), 'IndexedDB sync');
    await write.close();
    const read = await open({ server, context, query: 'args=--scenario%20tests/scenarios/save_read.lua&files=tests/scenarios/save_read.lua' });
    await waitForLog(read, /scenario 'save read': (passed|\d+ failure)/);
    assert.ok(read.logs.some((l) => /scenario 'save read': passed/.test(l)), read.logs.join('\n'));
    await read.close();
  } finally {
    await context.close();
    await server.close();
    fs.rmSync(dist, { recursive: true, force: true });
  }
});

// spec: rendering-core: Desktop and web agree
test('WebGL renders the reference scene like the desktop golden image', async () => {
  const g = await open({
    query: 'args=--scenario%20tests/scenarios/render_carpet.lua&files=tests/scenarios/render_carpet.lua,tests/golden/carpet.png',
  });
  try {
    const line = await waitForLog(g, /scenario 'reference scene': (passed|\d+ failure)|FAILED/, 90000);
    assert.match(line, /passed/, g.logs.join('\n'));
  } finally {
    await g.close();
  }
});

// spec: platform-targets: Missing content folder
test('missing content shows a page message', async () => {
  const dist = makeDist();
  fs.rmSync(path.join(dist, 'assets', 'index.tsv'));
  const g = await open({ dist });
  try {
    const text = await waitFor(async () => {
      const visible = await g.page.locator('#error').isVisible();
      return visible ? g.page.locator('#error').textContent() : null;
    }, 'the error message');
    assert.match(text, /content could not be loaded/);
  } finally {
    await g.close();
    fs.rmSync(dist, { recursive: true, force: true });
  }
});

// spec: netcode: Desktop host and browser client
test('a browser client flies with a native host', { skip: !HAS_NATIVE && `no native build at ${NATIVE_BIN}` }, async () => {
  const user = fs.mkdtempSync(path.join(os.tmpdir(), 'zb-host-'));
  const host = spawn(NATIVE_BIN, ['--headless', '--assets', path.join(REPO, 'assets'), '--user-dir', user,
    '--host', '0', '--scenario', 'tests/scenarios/coop_host.lua'], {
    cwd: REPO, env: { ...process.env, ZB_NO_DIALOGS: '1' }, stdio: ['ignore', 'pipe', 'pipe'],
  });
  let hostOut = '';
  host.stdout.on('data', (d) => { hostOut += d; });
  host.stderr.on('data', (d) => { hostOut += d; });
  const exited = new Promise((resolve) => host.on('exit', (code) => resolve(code)));
  let g;
  try {
    const port = await waitFor(async () => (hostOut.match(/hosting a co-op session on port (\d+)/) || [])[1],
      'the host to listen', 30000);
    const args = encodeURIComponent(`--join ws://127.0.0.1:${port} --scenario tests/scenarios/coop_client.lua`);
    g = await open({ query: `args=${args}&files=tests/scenarios/coop_client.lua` });
    const line = await waitForLog(g, /scenario 'co-op client': (passed|\d+ failure)|FAILED/, 180000);
    assert.match(line, /passed/, g.logs.join('\n'));
    const code = await Promise.race([exited, new Promise((r) => setTimeout(() => r('timeout'), 60000))]);
    assert.equal(code, 0, hostOut);
    assert.match(hostOut, /scenario 'co-op host': passed/);
  } finally {
    if (host.exitCode === null) host.kill();
    await g?.close();
    fs.rmSync(user, { recursive: true, force: true });
  }
});
