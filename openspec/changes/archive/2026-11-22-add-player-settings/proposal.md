# Proposal: The settings players expect

Milestone: M11 (polish after the 1.0 RC).

## Why

The mixer had master, music and effects groups, but no setting reached them. There was no V-sync,
frame cap, picture size or brightness setting, no invert look, and no way back to the defaults.
Larger text ran off the screen.

## What Changes

- New settings: volume, music, sounds (0 to 100 %), mute, V-sync, a frame cap (none, 30, 60 or
  120), the pixel resolution (240 to 720 lines), brightness (0.75 to 1.5) and invert look. Each
  applies at once and is kept.
- The engine: `engine.set_vsync`, `engine.set_frame_cap` and `render.brightness` (the image
  multiplied before the palette).
- Text lines wrap to the screen, so larger text stays readable; high contrast keeps its outline.
- "Reset to defaults" asks first and keeps the language.

## Platforms

Desktop: V-sync and the frame cap. Web: the browser paces frames, so both are ignored there.
Everything else is the same on every platform.

## Impact

New capability `settings`. `ui/settings.lua`, `ui/menu.lua`, `ui/text.lua`, `ui/access.lua`,
`main.lua`, `src/render/*`, `assets/base/shaders/post.frag`, `src/platform/*`, `src/app/engine.c`,
`src/script/script_api.c`, the German strings.
