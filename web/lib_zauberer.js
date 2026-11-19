// SPDX-License-Identifier: MIT
// JavaScript side of the platform layer, linked with --js-library.
addToLibrary({
  // Asks the page loader (web/zauberer-loader.js) to make an asset file
  // available in MEMFS, then reports back to zb_web_fetch_done(box, status)
  // with 0 (ready), 404 (not in the index) or another non-zero code.
  zb_js_fetch__deps: ['$UTF8ToString'],
  zb_js_fetch: function (pathPtr, box) {
    var path = UTF8ToString(pathPtr);
    var pending = Module['zbFetch'] ? Module['zbFetch'](path) : Promise.resolve(0);
    pending.then(function (status) {
      _zb_web_fetch_done(box, status | 0);
    }, function (error) {
      console.error('asset fetch failed: ' + path, error);
      _zb_web_fetch_done(box, 1);
    });
  },
});

addToLibrary({
  // Startup and fatal errors: a message on the page (desktop shows a dialog).
  zb_js_show_error__deps: ['$UTF8ToString'],
  zb_js_show_error: function (title, message) {
    if (Module['zbShowError']) {
      Module['zbShowError'](UTF8ToString(title), UTF8ToString(message));
    }
  },

  // A phone or tablet: coarse primary pointer and touch points (tests may override).
  zb_js_touch_primary: function () {
    if (typeof Module['zbTouchPrimary'] === 'boolean') {
      return Module['zbTouchPrimary'] ? 1 : 0;
    }
    var coarse = typeof matchMedia === 'function' && matchMedia('(pointer: coarse)').matches;
    var touch = typeof navigator !== 'undefined' && navigator.maxTouchPoints > 0;
    return coarse && touch ? 1 : 0;
  },

  // Persists /user (IDBFS) after configuration or save data changed.
  zb_js_sync_user_data: function () {
    if (typeof FS !== 'undefined' && FS.syncfs) {
      FS.syncfs(false, function (err) {
        if (err) console.error('user data sync failed', err);
      });
    }
  },
});

// WebXR bridge (immersive VR, design: openspec add-webxr-session). It reports
// whether an immersive-vr session is possible, starts one for the page's Enter
// VR button (Module.zbXrEnter) with a 'local' reference space and a WebGL layer
// on the engine's context, and drives the engine from the session's frames:
// per frame it writes the layer size, the viewer pose, each view's viewport,
// view matrix and projection, and the left and right controllers (grip or
// target-ray pose, thumbstick, trigger and squeeze) into wasm memory (layout:
// src/core/xr.h) and calls zb_web_xr_frame. Select and squeeze become engine
// input events. The page
// hears about changes through Module.zbXrStatus('unsupported' | 'supported' |
// 'running').
addToLibrary({
  $ZBXR__deps: ['$GL'],
  $ZBXR: {
    supported: false,
    session: null,
    space: null,
    framebuffer: 0,
    HANDS: { left: 1, right: 2 },
    STATUS: ['unsupported', 'supported', 'running'],

    report: function (status) {
      _zb_web_xr_status(status);
      if (Module['zbXrStatus']) Module['zbXrStatus'](ZBXR.STATUS[status]);
    },

    init: function () {
      Module['zbXrEnter'] = ZBXR.enter;
      Module['zbXrExit'] = ZBXR.exit;
      var xr = typeof navigator !== 'undefined' ? navigator.xr : undefined;
      if (!xr || typeof xr.isSessionSupported !== 'function' || typeof XRWebGLLayer === 'undefined') {
        ZBXR.report(0);
        return;
      }
      xr.isSessionSupported('immersive-vr').then(function (ok) {
        ZBXR.supported = !!ok;
        if (!ZBXR.session) ZBXR.report(ok ? 1 : 0);
      }, function () {
        ZBXR.report(0);
      });
    },

    // Called from a user gesture (the button). Resolves to whether a session started.
    enter: function () {
      if (ZBXR.session || !ZBXR.supported) return Promise.resolve(false);
      var gl = GL.currentContext ? GL.currentContext.GLctx : null;
      var session = null;
      return navigator.xr.requestSession('immersive-vr').then(function (s) {
        session = s;
        return gl.makeXRCompatible();
      }).then(function () {
        // No antialiasing: WebGL 2 cannot blit into a multisampled framebuffer,
        // and the retro look needs none. The engine writes no depth.
        var layer = new XRWebGLLayer(session, gl, { antialias: false, depth: false, alpha: false });
        session.updateRenderState({ baseLayer: layer });
        return session.requestReferenceSpace('local');
      }).then(function (space) {
        ZBXR.start(session, space);
        return true;
      }).catch(function (error) {
        console.error('WebXR: the session could not start: ' + error);
        if (session) session.end().catch(function () {});
        return false;
      });
    },

    exit: function () {
      return ZBXR.session ? ZBXR.session.end() : Promise.resolve();
    },

    start: function (session, space) {
      var layer = session.renderState.baseLayer;
      ZBXR.session = session;
      ZBXR.space = space;
      ZBXR.framebuffer = 0;
      if (layer.framebuffer) {
        // The layer's opaque framebuffer gets a GL name the engine can bind.
        var id = GL.getNewId(GL.framebuffers);
        layer.framebuffer.name = id;
        GL.framebuffers[id] = layer.framebuffer;
        ZBXR.framebuffer = id;
      }
      ['selectstart', 'selectend', 'squeezestart', 'squeezeend'].forEach(function (type) {
        session.addEventListener(type, ZBXR.onInput);
      });
      session.addEventListener('end', ZBXR.onEnd);
      ZBXR.report(2);
      session.requestAnimationFrame(ZBXR.onFrame);
    },

    onInput: function (event) {
      var hand = (event.inputSource && ZBXR.HANDS[event.inputSource.handedness]) || 0;
      var button = event.type.indexOf('squeeze') === 0 ? 1 : 0;
      _zb_web_xr_button(hand, button, /start$/.test(event.type) ? 1 : 0);
    },

    onFrame: function (time, frame) {
      var session = frame.session;
      if (session !== ZBXR.session) return;
      session.requestAnimationFrame(ZBXR.onFrame);
      var layer = session.renderState.baseLayer;
      var pose = frame.getViewerPose(ZBXR.space);
      if (!pose || !layer || pose.views.length < 1) return; // tracking lost: the compositor keeps the last frame
      var views = Math.min(pose.views.length, 2);
      var h = HEAPF32, at = _zb_web_xr_buffer() >> 2;
      var p = pose.transform.position, o = pose.transform.orientation;
      h[at] = views;
      h[at + 1] = layer.framebufferWidth;
      h[at + 2] = layer.framebufferHeight;
      h[at + 3] = p.x; h[at + 4] = p.y; h[at + 5] = p.z;
      h[at + 6] = o.x; h[at + 7] = o.y; h[at + 8] = o.z; h[at + 9] = o.w;
      for (var i = 0; i < views; ++i) {
        var view = pose.views[i], vp = layer.getViewport(view), v = at + 10 + i * 36;
        h[v] = vp.x; h[v + 1] = vp.y; h[v + 2] = vp.width; h[v + 3] = vp.height;
        h.set(view.transform.inverse.matrix, v + 4);
        h.set(view.projectionMatrix, v + 20);
      }
      var end = at + 10 + views * 36;
      ZBXR.writeControllers(frame, h, end);
      if (!_zb_web_xr_frame(ZBXR.framebuffer, 10 + views * 36 + 24)) {
        session.end().catch(function () {});
      }
    },

    // The controller block: per hand (left, right) tracked, position (3),
    // orientation (4), thumbstick x and y (forward +), trigger and squeeze.
    writeControllers: function (frame, h, at) {
      h.fill(0, at, at + 24);
      var sources = frame.session.inputSources || [];
      for (var i = 0; i < sources.length; ++i) {
        var src = sources[i], hand = ZBXR.HANDS[src.handedness];
        if (!hand) continue;
        var c = at + (hand - 1) * 12;
        var space = src.gripSpace || src.targetRaySpace;
        var pose = space ? frame.getPose(space, ZBXR.space) : null;
        if (pose) {
          var p = pose.transform.position, o = pose.transform.orientation;
          h[c] = 1;
          h[c + 1] = p.x; h[c + 2] = p.y; h[c + 3] = p.z;
          h[c + 4] = o.x; h[c + 5] = o.y; h[c + 6] = o.z; h[c + 7] = o.w;
        }
        var pad = src.gamepad;
        if (pad) {
          var axes = pad.axes || [], k = axes.length >= 4 ? 2 : 0;
          h[c + 8] = axes[k] || 0;
          h[c + 9] = -(axes[k + 1] || 0); // gamepads report forward as negative
          var b = pad.buttons || [];
          h[c + 10] = b[0] ? b[0].value || 0 : 0;
          h[c + 11] = b[1] ? b[1].value || 0 : 0;
        }
      }
    },

    onEnd: function (event) {
      if ((event.session || ZBXR.session) !== ZBXR.session) return;
      if (ZBXR.framebuffer) GL.framebuffers[ZBXR.framebuffer] = null;
      ZBXR.framebuffer = 0;
      ZBXR.session = null;
      ZBXR.space = null;
      ZBXR.report(ZBXR.supported ? 1 : 0);
    },
  },

  // Called by the platform layer once the engine's WebGL context exists.
  zb_js_xr_init__deps: ['$ZBXR'],
  zb_js_xr_init: function () {
    ZBXR.init();
  },
});
