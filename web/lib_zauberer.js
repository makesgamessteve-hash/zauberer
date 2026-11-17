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
