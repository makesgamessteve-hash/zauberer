# Proposal: WebXR Play

## Why

Milestone **M11 (Release)** makes immersive VR playable (concept §8.5, "WebXR"). The M1
spike `add-webxr-session` renders the world in stereo from a headset, reports the head
pose and turns select and squeeze into input actions, but a player in a headset can
neither fly the carpet nor cast: the controllers' poses, thumbsticks and triggers never
reach the game, nothing protects against motion sickness, and the HUD sits in the top
left corner of each eye at the same viewport position, where the eyes cannot fuse it
(the headset's frusta are asymmetric) and the lenses blur it.

## What Changes

- **Controllers in the engine:** each XR frame carries the left and right controller:
  tracked or not, the grip pose (else the target ray's) in the session's reference
  space, the thumbstick (x, and y with forward positive), trigger and squeeze. The
  bridge's flat float layout gains an optional controller block (12 floats per hand);
  frames without it (the M1 layout) still decode with both hands untracked. Tracked flags
  other than 0 and 1, non-finite numbers and tracked controllers without an orientation
  are rejected; axes and buttons are clamped. `web/lib_zauberer.js` reads
  `session.inputSources` (`gripSpace` or `targetRaySpace` through `frame.getPose`,
  `gamepad.axes` 2 and 3 when present, else 0 and 1, `gamepad.buttons[0]` and `[1]`).
- **Lua:** `xr.state().controllers.left|right` (`tracked`, `pose`, `axes`, `trigger`,
  `squeeze`) and the test API `xr.emulate_controller(hand, pose|false, axes?, trigger?,
  squeeze?)` for the emulated headset.
- **Rendering:** `render.vignette(strength)` darkens the edges of every view around its
  optical centre in the post pass (CPU reference `zb_vignette_factor`);
  `render.xr_hud(distance, pixel?)` puts screen text in headset views on a head-locked
  plane in front of the player, so both eyes see it in the same direction. Screen text in
  headset views is now drawn after the upscale, at the headset's resolution.
- **Game layer** `scripts/xr/play.lua` (pure Lua, options from the caller): steering by
  tilting a controller like reins (roll turns, pitch climbs) or by head direction,
  relative to a neutral pose taken at the session start and after a grip squeeze held for
  a second; the left thumbstick is the throttle (y) and adds turning (x), the right one's
  y adds climbing; the triggers charge and cast the hands' weaves while the controllers'
  paths are recorded, projected onto the plane facing the player and recognised as
  gestures that shape that hand's charge (or make the free weave's form); comfort
  options: snap turning (30° steps with a cooldown), reduced speed, the horizon
  stabiliser (the seat stays level) and the comfort vignette while turning or speeding
  up; the HUD lines as a wrapped panel low ahead, with a shadow.
- **Flight and magic:** the carpet's input takes `snap` (an instant heading change) and
  `scale` (a top-speed factor), carried by co-op inputs; `magic.play.gesture(p, g, hand)`
  is the one place a recognised gesture reaches the magic, for the mouse and for VR.
- **main.lua:** a few isolated lines: the VR input added to the flight input and the
  spell states, the seat as the camera and the vignette in the frame hook, the HUD panel
  in VR.

## Capabilities

### New Capabilities

- `webxr-play`: controller data from the headset, flight steering in VR, gestures in the
  air and casting with them, comfort options and a HUD that can be read in the headset.

### Modified Capabilities

None. `webxr-session` keeps its requirements: its frame layout is still accepted, and
screen text still fills every view at the same place unless `render.xr_hud` is set.

## Impact

- Code: `src/core/xr` (controllers, decoding, idle controllers), `src/app/xr.c`
  (emulated controllers), `src/script/script_xr.c`, `src/script/script_api.c`
  (`render.vignette`, `render.xr_hud`), `src/render` (text matrices per headset view,
  the vignette, text after the upscale), `assets/base/shaders/text.vert` and
  `post.frag`, the bridge in `web/lib_zauberer.js`; `assets/base/scripts/xr/play.lua`,
  small additions to `flight/carpet.lua`, `flight/coop.lua`, `magic/play.lua` and
  `main.lua`.
- Platforms: WebXR in the web builds is the only place a real session runs; desktop and
  headless builds run the same code, with the emulated headset in scenarios.
- Replays: like the head pose, controller poses are presentation data and are not
  recorded; a recording made in VR replays without its VR steering. Co-op carries the
  resulting flight input (snaps included), not the poses.
- Not yet: grabbing objects and flipping spellbook pages by hand, the spatial bag,
  aiming weaves with the controller, multiview rendering, and the settings menu for the
  comfort options (the caller passes them as a table, `game.xr_options`).
