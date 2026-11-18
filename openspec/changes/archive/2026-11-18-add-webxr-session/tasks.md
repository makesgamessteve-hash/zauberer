# Tasks

## 1. Frame data and events

- [x] 1.1 Add `core/xr` (frame, view and pose types, `zb_xr_frame_decode`, the emulated rig `zb_xr_emulate`) and the XR events in `core/event.h`; verify "webxr-session: Bad frame data" in a core unit test

## 2. Renderer

- [x] 2.1 Add `zb_render_set_views` and `zb_render_set_target`: views composed with the camera, the camera's depth range, per-eye internal rectangles and destination viewports, per-view culling; verify "Views from the headset" in a render unit test
- [x] 2.2 Present each eye into its viewport of the target, draw text per eye, read back from the target, and add the off-axis term to the sky shader; keep the M0 and M1 goldens unchanged

## 3. Input

- [x] 3.1 Parse `xr:` bindings and keep XR button state per hand; verify "Select presses an action" in an input unit test

## 4. Platform, engine and scripts

- [x] 4.1 Add the platform's XR handler and the exported bridge entry points (web), and the engine glue (`src/app/xr.c`): XR frames run the engine step, the window loop skips during a session, availability and session changes, the `xr_session` event
- [x] 4.2 Expose `xr.state()` and the test helpers `xr.emulate` and `xr.emulate_button`; verify "No session on the desktop", "Head pose reaches Lua" and "Select presses an action" with a headless scenario and "Emulated headset on the desktop" with a rendered scenario and golden image; regenerate the API reference

## 5. Web

- [x] 5.1 Implement the JS bridge (session, layer, reference space, frame loop, input sources, end) and the Enter VR button; report XR state to test drivers
- [x] 5.2 Add Playwright tests with a mock of `navigator.xr`: verify "No button without support", "Enter and leave", "Two views in the XR layer", "Head pose reaches Lua" and "Select presses an action"

## 6. Verification

- [x] 6.1 Map every scenario to a tagged test (`tools/spec_coverage.py`), keep C coverage ≥ 95 % and run the format, include, API reference, REUSE and luacheck gates and the web builds
