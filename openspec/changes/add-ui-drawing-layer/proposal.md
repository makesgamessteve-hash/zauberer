# Proposal: A 2D drawing layer for the UI

Milestone: UI redesign (after the 1.0 RC). First of six changes; see docs/design/ui/README.md.

## Why

The engine can draw models, terrain, sprites in the world and text (`render.draw_text`), and nothing
else in 2D. Every HUD element and menu is therefore a line of text, which is why the HUD is a list of
15 to 30 floating lines and the menus are lists of words. Frames, bars, icons, a mana ring, a painted
map and hit areas for the mouse and fingers all need 2D drawing the scripts can use.

## What Changes

- The renderer gains a UI pass, drawn after the world and post-processing, on the **UI grid**: the
  internal resolution (640×360 by default) scaled to the window by a whole number, so UI pixels are as
  crisp as the world's.
- New script functions:
  - `render.ui_rect(x, y, w, h, colour)`: a filled rectangle; `render.ui_frame(...)`: an outline.
  - `render.ui_image(texture, x, y, opts)`: an image or one cell of an atlas, with tint, alpha and a
    whole-number scale.
  - `render.ui_nine(texture, slice, x, y, w, h, opts)`: a nine-slice frame (corners kept, edges and
    middle tiled, not stretched, so ornament stays whole).
  - `render.ui_text(font, text, x, y, opts)`: text on the UI grid with a shadow or outline.
  - `render.ui_clip(x, y, w, h)` and `render.ui_unclip()`: clipping for scrolled lists.
  - `render.texture_from_pixels(w, h, rgba)` and `render.update_texture(...)`: textures made by
    scripts (the painted map).
- A Lua layout kit `ui/layout.lua`: the UI grid's size, the safe area (notches, rounded corners),
  anchored boxes (corners, edges, middle), the UI scale setting, and hit testing so the mouse and
  touch can point at boxes.
- Draw calls are batched per texture; a frame's UI is at most a few dozen draw calls.

## Platforms

All the same through the GL 3.3 / GLES 3.0 common subset. In WebXR the UI grid is drawn onto the HUD
panel that floats low ahead (the existing XR HUD path).

## Co-op

Local only: each heir's screen draws its own UI.

## Impact

New capability `ui-drawing`. `src/render/` (a UI pass and batcher in the backends, the null backend
records calls for tests), `src/script/script_render.c` (the new functions, documented for
docs/api), `assets/base/scripts/ui/layout.lua`. `render.draw_text` stays for compatibility.
