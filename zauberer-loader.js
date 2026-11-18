// SPDX-License-Identifier: MIT
// Page loader of the web build: picks the threaded or single-threaded build,
// mounts IndexedDB-backed user data, loads the asset index, prefetches the
// boot set through Cache Storage and fetches other assets on demand.
'use strict';

(function () {
  var params = new URLSearchParams(location.search);
  var CACHE_NAME = 'zauberer-assets-v1';
  var stats = { network: [], cache: [], synced: 0 };
  var index = new Map(); // "base/models/carpet.glb" -> { size, sha256, boot, loaded }
  var cachePromise = null;

  var isolated = self.crossOriginIsolated === true;
  var build = params.get('build') || (isolated ? 'zauberer' : 'zauberer-st');
  window.zbBuild = build;

  function $(id) { return document.getElementById(id); }

  function showError(title, message) {
    var box = $('error');
    box.textContent = title + ': ' + message;
    box.hidden = false;
    $('status').hidden = true;
  }

  function openCache() {
    if (!cachePromise) {
      cachePromise = (self.caches && self.isSecureContext)
        ? caches.open(CACHE_NAME).catch(function () { return null; })
        : Promise.resolve(null);
    }
    return cachePromise;
  }

  function mkdirs(FS, path) {
    var parts = path.split('/');
    var dir = '';
    for (var i = 0; i < parts.length - 1; ++i) {
      dir += (dir ? '/' : '') + parts[i];
      try { FS.mkdir(dir); } catch (e) { /* exists */ }
    }
  }

  // Bytes of one indexed asset: Cache Storage first, then the network.
  async function assetBytes(rel, entry) {
    var url = 'assets/' + rel + '?v=' + entry.sha256;
    var cache = await openCache();
    if (cache) {
      var hit = await cache.match(url);
      if (hit) {
        stats.cache.push(rel);
        return new Uint8Array(await hit.arrayBuffer());
      }
    }
    var response = await fetch(url, { cache: 'no-store' });
    if (!response.ok) {
      throw new Error('HTTP ' + response.status + ' for ' + url);
    }
    stats.network.push(rel);
    if (cache) {
      await cache.put(url, response.clone());
    }
    return new Uint8Array(await response.arrayBuffer());
  }

  async function loadIndex(FS) {
    var response = await fetch('assets/index.tsv', { cache: 'no-store' });
    if (!response.ok) {
      throw new Error('the asset index assets/index.tsv could not be loaded (HTTP ' + response.status + ')');
    }
    var lines = (await response.text()).split('\n');
    var boot = [];
    lines.forEach(function (line) {
      if (!line || line.charAt(0) === '#') return;
      var f = line.split('\t');
      var entry = { size: +f[1], sha256: f[2], boot: f[3] === '1', loaded: false };
      index.set(f[0], entry);
      mkdirs(FS, 'assets/' + f[0]);
      if (entry.boot || entry.size === 0) {
        boot.push(f[0]);
      } else {
        FS.writeFile('assets/' + f[0], new Uint8Array(0)); // placeholder until fetched
      }
    });
    await Promise.all(boot.map(async function (rel) {
      var entry = index.get(rel);
      FS.writeFile('assets/' + rel, entry.size ? await assetBytes(rel, entry) : new Uint8Array(0));
      entry.loaded = true;
    }));
    // Drop cached versions that the index no longer names.
    var cache = await openCache();
    if (cache) {
      var keys = await cache.keys();
      await Promise.all(keys.map(function (request) {
        var u = new URL(request.url);
        var rel = u.pathname.replace(/^.*?\/assets\//, '');
        var entry = index.get(rel);
        return entry && entry.sha256 === u.searchParams.get('v') ? null : cache.delete(request);
      }));
    }
  }

  // Extra files for tests (?files=a,b): fetched into MEMFS at the same path.
  async function loadExtraFiles(FS) {
    var list = (params.get('files') || '').split(',').filter(Boolean);
    await Promise.all(list.map(async function (rel) {
      var response = await fetch(rel, { cache: 'no-store' });
      if (!response.ok) throw new Error('HTTP ' + response.status + ' for ' + rel);
      mkdirs(FS, rel);
      FS.writeFile(rel, new Uint8Array(await response.arrayBuffer()));
    }));
  }

  function mountUserData(FS) {
    return new Promise(function (resolve) {
      try { FS.mkdir('/user'); } catch (e) { /* exists */ }
      FS.mount(FS.filesystems.IDBFS, {}, '/user');
      FS.syncfs(true, function (err) {
        if (err) console.warn('user data could not be restored', err);
        resolve();
      });
    });
  }

  // Called by the engine (zb_js_fetch) for any file it reads.
  function zbFetch(path) {
    var rel = path.replace(/^\/?assets\//, '');
    var entry = path === rel ? null : index.get(rel);
    var FS = Module.FS;
    if (!entry) {
      return Promise.resolve(FS.analyzePath(path).exists ? 0 : 404);
    }
    if (entry.loaded) {
      return Promise.resolve(0);
    }
    if (!entry.promise) {
      entry.promise = assetBytes(rel, entry).then(function (bytes) {
        FS.writeFile('assets/' + rel, bytes);
        entry.loaded = true;
        return 0;
      });
      entry.promise.catch(function () { entry.promise = null; });
    }
    return entry.promise;
  }

  function logLine(text) {
    if (/^\[error\]/.test(text)) console.error(text);
    else if (/^\[warn\]/.test(text)) console.warn(text);
    else console.log(text);
  }

  var canvas = $('canvas');
  canvas.addEventListener('webglcontextlost', function (e) {
    e.preventDefault();
    showError('Zauberer', 'The graphics context was lost. Reload the page to continue.');
  });

  // First user gesture: allow audio (browser autoplay rules).
  var unlocked = false;
  function unlockAudio() {
    if (unlocked || !Module.calledRun) return;
    unlocked = true;
    ['pointerdown', 'keydown', 'touchend'].forEach(function (t) { window.removeEventListener(t, unlockAudio, true); });
    Module._zb_web_audio_unlocked();
    setTimeout(function () {
      var ctx = Module.SDL3 && Module.SDL3.audioContext;
      if (ctx && ctx.state === 'suspended') ctx.resume();
    }, 100);
  }
  ['pointerdown', 'keydown', 'touchend'].forEach(function (t) { window.addEventListener(t, unlockAudio, true); });

  // WebXR: the engine's bridge (web/lib_zauberer.js) reports whether a headset
  // can present; the button starts an immersive session (a user gesture).
  var xrButton = $('xr');
  xrButton.addEventListener('click', function () {
    if (Module.zbXrEnter) Module.zbXrEnter();
  });
  function xrStatus(status) {
    xrButton.hidden = status !== 'supported';
  }

  var args = (params.get('args') || '').split(' ').filter(Boolean);

  window.Module = {
    canvas: canvas,
    arguments: args,
    print: logLine,
    printErr: logLine,
    zbFetch: zbFetch,
    zbStats: stats,
    zbShowError: showError,
    zbXrStatus: xrStatus,
    zbState: function () { return JSON.parse(UTF8ToStringSafe(Module._zb_web_state())); },
    preRun: [function () {
      var FS = Module.FS;
      Module.addRunDependency('zb-boot');
      Promise.all([mountUserData(FS), loadIndex(FS), loadExtraFiles(FS)]).then(function () {
        Module.removeRunDependency('zb-boot');
      }, function (error) {
        showError('Zauberer', 'The game content could not be loaded: ' + error.message);
      });
    }],
    onRuntimeInitialized: function () {
      $('status').hidden = true;
      var sync = Module.FS.syncfs;
      Module.FS.syncfs = function (populate, callback) {
        return sync.call(Module.FS, populate, function (err) {
          if (!populate && !err) stats.synced++;
          if (callback) callback(err);
        });
      };
    },
  };

  function UTF8ToStringSafe(ptr) { return Module.UTF8ToString(ptr); }

  var script = document.createElement('script');
  script.src = build + '.js';
  script.onerror = function () { showError('Zauberer', 'The engine (' + build + '.js) could not be loaded.'); };
  document.body.appendChild(script);
})();
