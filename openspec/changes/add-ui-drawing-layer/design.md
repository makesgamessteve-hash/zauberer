# Design: the UI drawing layer

## The UI grid

The world is rendered at the internal resolution (default 640×360) and scaled up with nearest
filtering. The UI is drawn in the same pixel units, so a 1-pixel ornament line is as thick as a pixel
of the world:

- `ui_scale = floor(min(window_w / internal_w, window_h / internal_h))`, at least 1;
- the grid is `window / ui_scale` pixels (a 1920×1080 window at 640×360 gives scale 3 and a 640×360
  grid; 2560×1440 gives scale 4 and a 640×360 grid; 1366×768 gives scale 2 and a 683×384 grid);
- the settings' UI scale (1, 2 or 3, from `add-ui-mood-and-feedback`) multiplies the grid's pixel
  size for people who want it larger, making the grid smaller.

The UI pass draws into the window's framebuffer after post-processing with an orthographic projection
of the grid, point sampling and alpha blending, so palette quantisation and dithering of the world do
not touch it (UI colours are exact).

## Batching

Calls are appended to one vertex buffer (position, UV, colour) per frame and split into batches when
the texture or the clip rectangle changes. Fonts, the UI atlas and the white texel used for
rectangles live in one or two textures, so a HUD frame is a handful of batches. The null backend
records the calls (kind, box, texture, colour) so tests can assert what was drawn without a GPU.

## Nine-slice

A nine-slice texture names its border widths (left, top, right, bottom). Edges and the middle are
tiled in whole repeats and the remainder is filled from the start of the pattern, so a kilim band
never shows a squashed diamond. Boxes smaller than the corners draw the corners only.

## Script textures

`render.texture_from_pixels(w, h, rgba_string)` uploads an RGBA8 texture (at most 1024×1024) and
returns a handle; `render.update_texture(handle, x, y, w, h, rgba_string)` replaces a region (the map
paints cells as they are seen). Textures made by scripts are freed with `render.free_texture`.

## Layout kit

`ui/layout.lua` works in grid pixels:

- `layout.grid()` gives the grid's width and height and `layout.safe()` the safe box (the platform's
  insets converted to the grid, at least 6 pixels from each edge);
- `layout.anchor(name, w, h, dx, dy)` places a box at one of nine anchors of the safe box;
- `layout.hit(box)` says whether the mouse or a touch is in a box this frame, and `layout.pressed(box)`
  whether it was clicked or tapped (a pressed box takes the touch, so the flight controls under it
  do not);
- `layout.stack(...)` lays boxes out down or across with a gap.

## Alternatives considered

- **Drawing frames with glyphs of a box font:** no curves, no images, no clipping; ornament would
  stay crude.
- **An HTML overlay on the web build:** two UIs to keep alike, nothing on desktop or in XR.
- **A third-party immediate-mode GUI (Nuklear, microui):** their look and input model would fight
  the pixel grid and the gamepad and touch navigation the game already has; the layer needed is small.
