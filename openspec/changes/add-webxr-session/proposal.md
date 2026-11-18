# Proposal: WebXR Session

## Why

Milestone **M1 (First Flight)** ends with "an XR headset (or emulation) shows the world
in stereo". The renderer has had a view list since M0 (`--debug-views 2`), but nothing
drives it from a headset. This change is a spike that locks the architecture in early:
how the browser's XR frames drive the engine, how headset views reach the renderer,
where the head pose lives for scripts and how controllers become input, so that later
XR work (comfort options, controller steering, gestures in the air, spatial UI) is
additive.

## What Changes

- **JS bridge** (`web/lib_zauberer.js`, our own MIT code): detects `immersive-vr`
  support, shows an **Enter VR** button in the page shell, starts a session with a
  `local` reference space and an `XRWebGLLayer` on the engine's WebGL 2 context (made XR
  compatible), and drives the engine from `session.requestAnimationFrame` while the
  session runs. Per frame it writes the layer framebuffer size, each view's viewport,
  view matrix and projection, and the viewer pose into wasm memory and calls the engine.
  Select and squeeze events of input sources become engine input events. When the
  session ends, the normal loop resumes.
- **Engine**: an XR frame runs the normal engine step (ticks, script hooks, terrain,
  rendering) with the headset's views; the normal iterate step skips while a session
  runs (implementation plan §1.2).
- **Renderer**: explicit views replace the camera-derived ones. Each headset view is
  applied relative to the camera that scripts set (the session origin: the seat on the
  carpet), with the headset's field of view and the camera's near and far planes. The
  scene renders per eye at the internal resolution, and each eye is upscaled with
  nearest-neighbour filtering into its viewport of the XR layer's framebuffer. The sky
  supports asymmetric (off-axis) projections.
- **Input**: bindings `xr:select`, `xr:squeeze`, `xr:left_select`, `xr:right_select`,
  `xr:left_squeeze` and `xr:right_squeeze`.
- **Lua**: `xr.state()` (availability, session, views, head pose relative to the session
  origin), an `xr_session` event, and the test helpers `xr.emulate` and
  `xr.emulate_button`, which emulate a headset in scenarios on every platform.

## Capabilities

### New Capabilities

- `webxr-session`: entering and leaving immersive VR on the web, stereo rendering from
  headset views relative to the camera, the head pose for scripts, controller buttons as
  input actions, validation of the bridge's frame data, the inactive path on desktop and
  the emulated headset for tests.

### Modified Capabilities

None. The multi-view requirement of `rendering-core` already asks for views with their
own camera matrices and viewports; the forced-stereo path is unchanged.

## Impact

- Code: `src/core/xr` (frame data, decoding, emulated headset) and two XR events in
  `src/core/event.h`; bridge entry points and an XR handler in `src/platform`; explicit
  views, a render target and per-view upscaling in `src/render`; XR bindings in
  `src/input`; `src/script/script_xr.c`; `src/app/xr.c` with a few calls in
  `src/app/engine.c`; the bridge and button in `web/`; an off-axis term in
  `assets/base/shaders/sky.frag`.
- Platforms:
  - **WebXR** (web builds in a browser with an `immersive-vr` device, including
    standalone headsets' browsers) is the only place a real session runs.
  - **Desktop** and **headless** builds compile the same code; no session starts there
    except the emulated one in scenarios.
  - **Web** and **mobile web** without an XR device show no button.
  - Multiview (`OVR_multiview2`) is not used yet: one pass per view.
- Co-op: rendering and head tracking are local, and nothing is replicated. Controller
  buttons become ordinary input actions, which travel like any other input once the
  netcode exists.
