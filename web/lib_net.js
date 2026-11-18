// SPDX-License-Identifier: MIT
// The browser's (or Node.js's) WebSocket for the engine's network transport
// (src/net/transport.c). Messages are queued here and copied into the wasm
// heap when the engine polls.
addToLibrary({
  $ZbSockets: { list: [] },

  zb_js_ws_open__deps: ['$ZbSockets', '$UTF8ToString'],
  zb_js_ws_open: function (urlPtr) {
    var Ctor = typeof WebSocket !== 'undefined' ? WebSocket : null;
    if (!Ctor) {
      return -1;
    }
    var entry = { ws: null, queue: [], state: 0 };
    try {
      entry.ws = new Ctor(UTF8ToString(urlPtr));
    } catch (e) {
      return -1;
    }
    entry.ws.binaryType = 'arraybuffer';
    entry.ws.onopen = function () { entry.state = 1; };
    entry.ws.onclose = function () { entry.state = 2; };
    entry.ws.onerror = function () { entry.state = 2; };
    entry.ws.onmessage = function (ev) {
      if (ev.data instanceof ArrayBuffer) {
        entry.queue.push(new Uint8Array(ev.data));
      }
    };
    ZbSockets.list.push(entry);
    return ZbSockets.list.length - 1;
  },

  zb_js_ws_state__deps: ['$ZbSockets'],
  zb_js_ws_state: function (id) {
    var e = ZbSockets.list[id];
    return e ? e.state : 2;
  },

  zb_js_ws_send__deps: ['$ZbSockets'],
  zb_js_ws_send: function (id, ptr, len) {
    var e = ZbSockets.list[id];
    if (!e || e.state !== 1) {
      return -1;
    }
    e.ws.send(HEAPU8.slice(ptr, ptr + len));
    return 0;
  },

  zb_js_ws_peek__deps: ['$ZbSockets'],
  zb_js_ws_peek: function (id) {
    var e = ZbSockets.list[id];
    return e && e.queue.length ? e.queue[0].length : -1;
  },

  zb_js_ws_take__deps: ['$ZbSockets'],
  zb_js_ws_take: function (id, dst) {
    var msg = ZbSockets.list[id].queue.shift();
    HEAPU8.set(msg, dst);
  },

  zb_js_ws_close__deps: ['$ZbSockets'],
  zb_js_ws_close: function (id) {
    var e = ZbSockets.list[id];
    if (e && e.state !== 2) {
      e.state = 2;
      try { e.ws.close(); } catch (err) { /* already closing */ }
    }
  },
});
