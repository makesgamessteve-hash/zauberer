# Design: WebXR Session

## Context

The render frontend builds draw lists per view (`zb_frame.views`, at most two), and the
GL backend renders each view into its rectangle of a low-resolution internal
framebuffer, runs the palette pass, draws text and blits the result to the window.
`--debug-views 2` already splits the internal framebuffer into two eyes derived from the
camera. The web build runs SDL3's main callbacks on the browser's main thread
(`requestAnimationFrame`), in a threaded and a single-threaded variant. Concept §8.5
asks for a seated carpet experience in which the carpet is the reference frame, and the
implementation plan (§1.2, §1.10) for a small JavaScript bridge of our own that drives
the engine from the XR frame callback.

## Goals / Non-Goals

**Goals:** a real `immersive-vr` session on the web with head-tracked stereo in the
retro look; a frame flow, data hand-off and module split that later XR work extends
rather than replaces; the head pose for scripts; controller buttons as input actions;
automated tests on every push without a headset.

**Non-Goals:** controller poses and rays, thumbsticks, haptics, comfort options
(vignette, snap turning), spatial UI and gestures in the air (later milestones); the
`OVR_multiview2` single-pass path; `local-floor` and bounded reference spaces; AR.

## Decisions

### 1. XR frames drive the engine step

While a session runs, browsers stop or throttle the window's `requestAnimationFrame` on
headsets, so the bridge drives the engine from `session.requestAnimationFrame`, as the
implementation plan prescribes. `zb_engine_iterate` asks the XR glue (`src/app/xr.c`)
first: while a device session runs and no XR frame is in progress, it returns at once.
The XR frame entry (`zb_engine_xr_frame`) applies the frame's views and head pose, sets
an "in XR frame" flag and runs the same `zb_engine_iterate`: ticks from the real-time
accumulator, scenario steps, script hooks, terrain and rendering, without a window swap
(the browser presents the XR layer). The flag also keeps the step from finishing the
run inside an XR callback: a quit ends the session (the frame entry returns false and
the bridge calls `session.end()`), and the next window iteration finishes normally.

Platform events (keyboard, mouse, gamepads) still arrive through SDL's queue; XR events
come through a handler the app registers with the platform (`zb_platform_set_xr_handler`),
because SDL's event pump only runs in the window loop. The platform remembers the last
availability report and delivers it when a handler is set.

### 2. Frame hand-off

The bridge writes one frame as floats into a static buffer that the platform exports
(`zb_web_xr_buffer`) and calls `zb_web_xr_frame(framebuffer, floats)`:

| Offset | Content |
| --- | --- |
| 0 | view count (1 or 2) |
| 1, 2 | layer framebuffer width and height |
| 3–5 | viewer position (metres, reference space) |
| 6–9 | viewer orientation (x, y, z, w) |
| 10 + 36 i | view i: viewport x, y, width, height |
| 14 + 36 i | view i: view matrix (`view.transform.inverse.matrix`, column-major) |
| 30 + 36 i | view i: projection matrix |

`zb_xr_frame_decode` (in `core`, so it is unit-tested on every platform) validates the
data: one or two views, positive sizes, viewports inside the framebuffer and finite
numbers. Rejected data is logged once and the frame is skipped. One flat buffer and one
call per frame avoid per-frame allocations and keep the JavaScript side trivial. The
layer's opaque framebuffer is registered in Emscripten's GL object table, so the C
backend binds it by an ordinary framebuffer name.

### 3. The camera is the session origin

The session uses the `local` reference space: its origin is where the head was when the
session started, with −Z forward and +Y up, which matches the camera's eye space. The
renderer therefore composes each view as `view = headset_view × camera_view`, where
`camera_view` is the look-at matrix of the camera that scripts set. A script that keeps
the camera at the seat on the carpet, level and facing the carpet's heading, gets the
seated experience with the carpet as the reference frame and no XR-specific code;
existing scenes work unchanged. The headset's projection keeps its field of view
(including the asymmetric frustum of real lenses), but its depth terms are rebuilt from
the camera's near and far planes, because the browser's default far plane (1,000 m)
would clip the terrain. The eye position, right and up vectors for fog, sprites and the
sky come from the composed view matrix, and frustum culling uses the composed
view-projection per view. The sky shader adds the projection's off-axis offsets
(`m[8]/m[0]`, `m[9]/m[5]`), which are zero for every camera-derived view, so the
existing golden images are unchanged.

### 4. Retro look per eye

Each eye renders at the internal height (default 360 pixels) with the width of its
viewport's aspect ratio; the eyes sit side by side in the internal framebuffer, so the
sky, terrain, model, sprite and palette passes are the same as on a flat screen. Text
is drawn into every eye at the same position. The presentation step blits each eye's
rectangle into its viewport of the target framebuffer with nearest-neighbour filtering,
and leaves the rest of the target alone (the browser clears the layer every frame). The
layer is created without antialiasing: a WebGL 2 blit into a multisampled framebuffer
is invalid, and the retro look needs no antialiasing. The render target is set with
`zb_render_set_target` and the explicit views with `zb_render_set_views`; both stay in
effect until they are cleared when the session ends.

### 5. Controller buttons

The bridge maps `selectstart`, `selectend`, `squeezestart` and `squeezeend` of an input
source to `ZB_EV_XR_BUTTON` events with the hand (`handedness` left or right, otherwise
none), the button and its state. The input module keeps the raw state per hand and
button like keys, so an action bound to `xr:select` sees a press that started and ended
between two ticks, and recordings and replays treat XR buttons like any other input.
Default bindings stay with the game's control changes (add-carpet-flight).

### 6. Scripts

`xr.state()` reads the engine's XR state through a host callback: available, active,
emulated, views, XR frames in the session, the layer size and the head pose relative to
the session origin. The pose is updated before the frame hooks of each XR frame. A
start or end of a session reaches scripts as the event `xr_session` (value `started` or
`ended`), delivered at the start of the next engine iteration so that it never runs
inside another hook or a scenario step.

### 7. Emulated headset

The test API (`xr.emulate`, `xr.emulate_button`) runs the same engine path without a
browser: an emulated session's frames come from `zb_xr_emulate`, a two-view rig with
eyes 64 mm apart along the head's right axis and real-lens-like asymmetric frusta
(50° outwards, 40° inwards, and a vertical field of view that matches the viewport's
aspect ratio), rendering side by side into the window (framebuffer 0). It lets headless and rendered scenarios on every platform test the XR
glue, the renderer's explicit views and the scripts' view of the session. The web E2E
test injects a mock of `navigator.xr` instead and exercises the real bridge.

### 8. Module boundaries

- `core/xr`: frame data, decoding and the emulated rig (pure C, no dependencies).
- `platform`: the exported bridge entry points and the handler; only it sees
  Emscripten.
- `render`: explicit views and the target; it knows nothing about sessions.
- `input`: XR button bindings; `script`: the `xr` table; `app`: session state and the
  frame flow, wiring them together.

## Determinism

Nothing here generates or simulates the world. Ticks still come from the fixed-step
accumulator, so XR frames at 72–120 Hz run the same ticks as window frames. Controller
buttons are input actions and are recorded and replayed. The head pose is presentation
data: it is not part of the state hash or recordings, so gameplay must not depend on it
until a later change records it as input.

## Risks / Trade-offs

- [Two passes per frame cost GPU time at 72–90 Hz] → The internal resolution keeps the
  per-eye cost at the flat-screen level; multiview comes when profiling asks for it.
- [`makeXRCompatible` can lose and restore the context on multi-GPU systems] → Rare on
  headsets (single GPU); the page already reports a lost context.
- [Text drawn into both eyes at zero disparity] → Acceptable for development overlays;
  in-game XR UI will be spatial.
- [Real devices differ from the mock] → The mock follows the WebXR API surface the
  bridge uses; a manual check on a headset is part of the milestone demo.
