# Tasks

## 1. Controller data

- [x] 1.1 Add controllers to `zb_xr_frame` and the optional controller block to the bridge layout and `zb_xr_frame_decode` (the M1 layout still accepted, bad data rejected, axes and buttons clamped); verify "webxr-play: Controllers reach Lua" and "webxr-play: Bad controller data" in a core unit test
- [x] 1.2 Read `inputSources` in `web/lib_zauberer.js` (grip or target-ray pose, thumbstick on axes 2/3 or 0/1, trigger and squeeze) and cover it in the Playwright XR test with the mocked `navigator.xr`
- [x] 1.3 Expose `xr.state().controllers` and the test API `xr.emulate_controller`, with the emulated controllers in the engine's emulated frames; regenerate the API reference

## 2. Rendering

- [x] 2.1 Add `render.vignette` (the post pass per view about its optical centre, CPU reference `zb_vignette_factor`); verify "webxr-play: Comfort vignette" in a unit test
- [x] 2.2 Add `render.xr_hud` (a head-locked text plane per headset view) and draw headset text after the upscale; verify "webxr-play: HUD readable in the headset" in a render unit test and a rendered scenario

## 3. Game layer

- [x] 3.1 Add `scripts/xr/play.lua`: reins and head steering with recentring, thumbsticks, snap turning, reduced speed, the stabilised seat, the vignette and the HUD panel; verify the steering and comfort scenarios in `tests/lua/test_xr_play.lua`
- [x] 3.2 Record strokes while a trigger is held, project them onto the plane facing the player and recognise them; give them to `magic.play.gesture` for the hand that drew; verify "webxr-play: Gestures in the air" and "webxr-play: Casting by drawing in the air"
- [x] 3.3 Carry `snap` and `scale` in the carpet's input and in co-op inputs; wire the module into `main.lua` with a few isolated lines

## 4. Verification

- [x] 4.1 Add the headless scenario `xr_play` (thumbstick, reins, a snap turn, a Fireball drawn in the air, the HUD) and its rendered variant; map every scenario to a tagged test and run the format, include, API reference, REUSE and luacheck gates and the web builds
