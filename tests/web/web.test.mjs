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
 * server to revisit the same origin (Cache Storage and IndexedDB are per origin),
 * and init = [fn, arg] to run a script in the page before any of its own. */
async function open({ query = '', isolated = false, context = null, dist = null, device = null, server = null,
  init = null } = {}) {
  const root = server ? server.root : (dist || makeDist());
  const ownServer = !server;
  server = server || await startServer(root, { isolated });
  const ctx = context || await browser.newContext(device ? { ...devices[device] } : {});
  const page = await ctx.newPage();
  if (init) await page.addInitScript(...init);
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

/** The emulated WebXR device: an 820 x 420 layer with two 400 x 400 viewports
 * and a 10 px border; the left eye looks ahead, the right one 60 degrees up;
 * the head is at (0.1, 0.2, -0.3), turned 30 degrees to the left. A right
 * controller with a grip pose and a four-axis gamepad (thumbstick on axes 2
 * and 3), a left one with only a target ray and a two-axis gamepad, and a
 * hand-less source that the bridge ignores. */
const XR_DEVICE = {
  size: [820, 420],
  head: { position: [0.1, 0.2, -0.3], orientation: [0, Math.sin(Math.PI / 12), 0, Math.cos(Math.PI / 12)] },
  views: [
    { eye: 'left', viewport: { x: 10, y: 10, width: 400, height: 400 }, position: [-0.032, 0, 0],
      orientation: [0, 0, 0, 1] },
    { eye: 'right', viewport: { x: 410, y: 10, width: 400, height: 400 }, position: [0.032, 0, 0],
      orientation: [Math.sin(Math.PI / 6), 0, 0, Math.cos(Math.PI / 6)] },
  ],
  controllers: [
    { handedness: 'right', grip: { position: [0.2, -0.3, -0.4], orientation: [0, 0, Math.sin(Math.PI / 12),
      Math.cos(Math.PI / 12)] }, axes: [0.9, 0.9, 0.5, -0.25], buttons: [0.75, 0.25] },
    { handedness: 'left', ray: { position: [-0.2, -0.3, -0.4], orientation: [0, 0, 0, 1] }, axes: [-0.5, 0.5],
      buttons: [0] },
    { handedness: 'none', ray: { position: [0, 0, 0], orientation: [0, 0, 0, 1] }, axes: [], buttons: [] },
  ],
};

/** Runs in the page before its scripts: a mock of navigator.xr with the parts
 * of the WebXR API the bridge uses. The layer's framebuffer is a real,
 * readable one that is cleared before every XR frame, like a browser's.
 * window.zbXrMock reads pixels, presses controller buttons and ends the session. */
function installXrMock(device) {
  const mock = { requests: [], spaces: [], frames: 0, handles: 0, compatible: false, layerOptions: null,
    session: null, layer: null, gl: null };

  // A rigid transform (column-major) from a position and a quaternion, with its inverse.
  function rigid(p, q) {
    const [x, y, z, w] = q;
    const r = [1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y),
      2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x),
      2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y)]; // columns
    const m = new Float32Array(16);
    const inv = new Float32Array(16);
    for (let c = 0; c < 3; ++c) {
      for (let k = 0; k < 3; ++k) {
        m[c * 4 + k] = r[c * 3 + k];
        inv[k * 4 + c] = r[c * 3 + k];
      }
      inv[12 + c] = -(r[c * 3] * p[0] + r[c * 3 + 1] * p[1] + r[c * 3 + 2] * p[2]);
    }
    m[12] = p[0];
    m[13] = p[1];
    m[14] = p[2];
    m[15] = inv[15] = 1;
    return { position: { x: p[0], y: p[1], z: p[2], w: 1 }, orientation: { x, y, z, w }, matrix: m,
      inverse: { matrix: inv } };
  }

  // 90 degrees in both directions, near 0.1, far 1000.
  const projection = new Float32Array([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1000.1 / 999.9, -1, 0, 0, -200 / 999.9, 0]);
  const pose = {
    transform: rigid(device.head.position, device.head.orientation),
    views: device.views.map((v) => ({ eye: v.eye, projectionMatrix: projection,
      transform: rigid(v.position, v.orientation) })),
  };
  // Input sources: spaces are tagged with the pose the frame reports for them.
  const inputSources = (device.controllers || []).map((c) => ({
    handedness: c.handedness, targetRayMode: 'tracked-pointer',
    gripSpace: c.grip ? { pose: c.grip } : undefined,
    targetRaySpace: { pose: c.ray || c.grip },
    gamepad: { axes: c.axes, buttons: c.buttons.map((value) => ({ value, pressed: value > 0.5 })) },
  }));
  const getPose = (space) => (space && space.pose ? { transform: rigid(space.pose.position,
    space.pose.orientation) } : null);

  function withLayer(fn) {
    const gl = mock.gl;
    const draw = gl.getParameter(gl.DRAW_FRAMEBUFFER_BINDING);
    const read = gl.getParameter(gl.READ_FRAMEBUFFER_BINDING);
    gl.bindFramebuffer(gl.FRAMEBUFFER, mock.layer.framebuffer);
    try {
      return fn(gl);
    } finally {
      gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, draw);
      gl.bindFramebuffer(gl.READ_FRAMEBUFFER, read);
    }
  }

  class MockLayer {
    constructor(session, gl, options) {
      mock.layerOptions = options || {};
      mock.gl = gl;
      this.framebufferWidth = device.size[0];
      this.framebufferHeight = device.size[1];
      const draw = gl.getParameter(gl.DRAW_FRAMEBUFFER_BINDING);
      const read = gl.getParameter(gl.READ_FRAMEBUFFER_BINDING);
      const texture = gl.getParameter(gl.TEXTURE_BINDING_2D);
      const color = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, color);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, device.size[0], device.size[1], 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
      this.framebuffer = gl.createFramebuffer();
      gl.bindFramebuffer(gl.FRAMEBUFFER, this.framebuffer);
      gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, color, 0);
      gl.bindTexture(gl.TEXTURE_2D, texture);
      gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, draw);
      gl.bindFramebuffer(gl.READ_FRAMEBUFFER, read);
      mock.layer = this;
    }

    getViewport(view) {
      return device.views.find((v) => v.eye === view.eye).viewport;
    }
  }

  class MockSession extends EventTarget {
    constructor() {
      super();
      this.renderState = { baseLayer: null, depthNear: 0.1, depthFar: 1000 };
      this.inputSources = inputSources;
      this.ended = false;
    }

    updateRenderState(state) {
      Object.assign(this.renderState, state);
    }

    requestReferenceSpace(type) {
      mock.spaces.push(type);
      return type === 'local' || type === 'viewer' ? Promise.resolve({ type })
        : Promise.reject(new DOMException(`${type} is not supported`, 'NotSupportedError'));
    }

    requestAnimationFrame(callback) {
      window.requestAnimationFrame((time) => {
        if (this.ended) return;
        if (mock.layer) {
          withLayer((gl) => {
            const clear = gl.getParameter(gl.COLOR_CLEAR_VALUE);
            gl.clearColor(0, 0, 0, 0);
            gl.clear(gl.COLOR_BUFFER_BIT);
            gl.clearColor(clear[0], clear[1], clear[2], clear[3]);
          });
        }
        mock.frames++;
        callback(time, { session: this, getViewerPose: () => pose, getPose });
      });
      return ++mock.handles;
    }

    end() {
      if (!this.ended) {
        this.ended = true;
        const event = new Event('end');
        event.session = this;
        this.dispatchEvent(event);
      }
      return Promise.resolve();
    }
  }

  const xr = new EventTarget();
  xr.isSessionSupported = (mode) => Promise.resolve(mode === 'immersive-vr');
  xr.requestSession = (mode) => {
    mock.requests.push(mode);
    if (mode !== 'immersive-vr') return Promise.reject(new DOMException(mode, 'NotSupportedError'));
    mock.session = new MockSession();
    return Promise.resolve(mock.session);
  };
  Object.defineProperty(Navigator.prototype, 'xr', { configurable: true, get: () => xr });
  window.XRWebGLLayer = MockLayer;
  WebGL2RenderingContext.prototype.makeXRCompatible = function () {
    mock.compatible = true;
    return Promise.resolve();
  };
  // RGBA of a layer pixel (origin bottom-left), as the last XR frame left it.
  mock.pixel = (x, y) => withLayer((gl) => {
    const px = new Uint8Array(4);
    gl.readPixels(x, y, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, px);
    return Array.from(px);
  });
  // A controller's select or squeeze, started and ended at once (between two ticks).
  mock.press = (handedness, button) => {
    for (const type of [`${button}start`, button, `${button}end`]) {
      const event = new Event(type);
      event.inputSource = { handedness, targetRayMode: 'tracked-pointer' };
      mock.session.dispatchEvent(event);
    }
  };
  mock.end = () => mock.session.end();
  window.zbXrMock = mock;
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

// spec: input-actions: The mouse is captured in the browser
test('a click captures the mouse; the capturing click is not the game\'s', async () => {
  const g = await open();
  try {
    await waitForFrames(g);
    const page = g.page;
    await page.evaluate(() => {
      window.zbDowns = 0;
      document.getElementById('canvas').addEventListener('pointerdown', () => { window.zbDowns++; });
    });
    assert.equal(await page.evaluate(() => document.getElementById('capture').hidden), false, 'the hint shows');
    await page.mouse.click(300, 300);
    await waitFor(() => page.evaluate(() => document.pointerLockElement === document.getElementById('canvas')),
      'the pointer lock');
    assert.equal(await page.evaluate(() => window.zbDowns), 0, 'the capturing click did not reach the game');
    assert.equal(await page.evaluate(() => document.getElementById('capture').hidden), true, 'the hint hides');
    await page.mouse.click(300, 300);
    assert.equal(await page.evaluate(() => window.zbDowns), 1, 'later clicks are the game\'s');
    await page.evaluate(() => document.exitPointerLock());
    await waitFor(() => page.evaluate(() => !document.getElementById('capture').hidden), 'the hint again');
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

// spec: webxr-session: No button without support
test('no Enter VR button without an immersive VR device', async () => {
  const g = await open();
  try {
    const s = await waitForFrames(g);
    assert.equal(s.xr.available, false);
    assert.equal(s.xr.active, false);
    assert.equal(await g.page.locator('#xr').isVisible(), false);
    assert.equal(await g.page.evaluate(() => Module.zbXrEnter()), false, 'no session starts');
    assert.deepEqual(g.errors, []);
  } finally {
    await g.close();
  }
});

/** Enters VR with the emulated device, checks the layer, a select and the end of the session. */
async function xrSession(isolated) {
  const g = await open({
    query: 'args=--scenario%20tests/scenarios/xr_web.lua&files=tests/scenarios/xr_web.lua',
    init: [installXrMock, XR_DEVICE],
    isolated,
  });
  try {
    assert.equal(await g.page.evaluate(() => window.zbBuild), isolated ? 'zauberer' : 'zauberer-st');
    const button = g.page.locator('#xr');
    await button.waitFor({ state: 'visible', timeout: 60000 });
    assert.equal((await g.state()).xr.available, true);
    await waitForLog(g, /xr web: waiting for the session/);
    await button.click();
    await waitForLog(g, /xr web: views checked|scenario 'webxr session'/, 120000);
    const s = await g.state();
    assert.equal(s.xr.active, true, g.logs.join('\n'));
    assert.ok(s.xr.frames >= 5);
    assert.equal(s.script_errors, 0);
    assert.equal(await button.isVisible(), false, 'no button while the session runs');
    const device = await g.page.evaluate(() => ({ requests: zbXrMock.requests, spaces: zbXrMock.spaces,
      compatible: zbXrMock.compatible, antialias: zbXrMock.layerOptions.antialias }));
    assert.deepEqual(device, { requests: ['immersive-vr'], spaces: ['local'], compatible: true, antialias: false });

    // The layer after an XR frame: each viewport shows the sky as its own view
    // sees it, and the border around the viewports stays as the browser cleared it.
    const px = await g.page.evaluate(() => ({
      left: zbXrMock.pixel(210, 210),
      right: zbXrMock.pixel(610, 210),
      border: [zbXrMock.pixel(5, 5), zbXrMock.pixel(815, 210), zbXrMock.pixel(410, 415), zbXrMock.pixel(210, 3)],
    }));
    assert.equal(px.left[3], 255, `left view drawn: ${px.left}`);
    assert.ok(px.left[0] > px.left[2] + 60, `the left view looks at the warm horizon: ${px.left}`);
    assert.equal(px.right[3], 255, `right view drawn: ${px.right}`);
    for (const [i, want] of [26, 51, 204].entries()) {
      assert.ok(Math.abs(px.right[i] - want) <= 16, `the right view looks up at the zenith: ${px.right}`);
    }
    assert.deepEqual(px.border, [[0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]]);

    await g.page.evaluate(() => zbXrMock.press('right', 'select'));
    await waitForLog(g, /xr web: select seen|scenario 'webxr session'/, 120000);
    await g.page.evaluate(() => zbXrMock.end());
    const line = await waitForLog(g, /scenario 'webxr session': (passed|\d+ failure)/, 120000);
    assert.match(line, /passed/, g.logs.join('\n'));
    assert.equal(await button.isVisible(), true, 'the button returns after the session');
    assert.deepEqual(g.errors, []);
  } finally {
    await g.close();
  }
}

// spec: webxr-session: Enter and leave
// spec: webxr-session: Two views in the XR layer
// spec: webxr-session: Head pose reaches Lua
// spec: webxr-session: Select presses an action
// spec: webxr-play: Controllers reach Lua
test('an emulated headset enters VR, renders both views and leaves again', () => xrSession(false));

test('the threaded build enters VR the same way', { skip: !HAS_MT && 'no threaded web build' },
  () => xrSession(true));
