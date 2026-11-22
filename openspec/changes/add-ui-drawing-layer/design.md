# Design: the UI drawing layer

## The UI grid is the internal framebuffer

The world is rendered into an internal framebuffer (as tall as the pixel resolution setting, 360 by
default, as wide as the window's aspect gives) and scaled up to the window. The existing text path
already draws into that framebuffer after the palette pass and before the upscale, in its pixels.
The UI layer uses the same path: UI pixels are the world's pixels, crisp at any window size, and
the palette quantisation never touches them.

`render.ui_grid()` gives the grid's width and height and where it lies in the window (normalised),
so pointer positions can be turned into grid pixels.

## Quads, batched

The text path's vertices (position, UV, colour) and batches (texture, first, count) carry every UI
draw:

- `render.ui_rect(x, y, w, h, rgba)` is a quad of a white texel (a 1×1 white texture made when the
  renderer starts);
- `render.ui_image(texture, x, y, opts)` is a quad of a texture or a part of it (`src = {x, y, w, h}`
  in texels), at `w`/`h` or the part's size times `scale`, tinted;
- `render.ui_nine(texture, x, y, w, h, opts)` draws a nine-slice frame from `src` and
  `border = {l, t, r, b}`: the corners whole, the edges and middle tiled in whole repeats with the
  last repeat cut (never stretched), and only the corners when the box is smaller than them;
- `render.draw_text(..., { shadow = rgba })` draws a one-pixel shadow under the text.

A quad with the same texture as the batch before it joins that batch, so a frame of rectangles,
frames from one atlas and text from one font is a few draw calls.

## Clipping on the CPU

`render.ui_clip(x, y, w, h)` pushes a clip rectangle (intersected with the one before), and
`render.ui_unclip()` pops it. Quads are clipped when they are added, cutting their UVs in proportion,
so the backends need no scissor and headset views keep working unchanged.

## Textures made by scripts

`render.texture_from_pixels(w, h, rgba)` makes a texture (at most 1024×1024, `rgba` exactly
`w * h * 4` bytes); the renderer keeps the pixels, so `render.update_texture(t, x, y, w, h, rgba)`
replaces a region and uploads the texture again, and `render.free_texture(t)` frees it. The painted
map updates a few cells a second, so re-uploading is cheap enough.

## Pointing

The platform normalises mouse positions to the window (0..1) like touches. `input.pointer()` gives
`{ x, y, held, pressed, released, wheel, touch }` in window units; the layout kit turns them into grid
pixels with `render.ui_grid()`. `input.claim(rects)` lists normalised rectangles the UI holds this
frame: a touch beginning inside one is not given to the on-screen sticks and buttons.

## Layout kit

`ui/layout.lua` works in grid pixels:

- `layout.grid()` and `layout.safe()` give the grid and the safe box (the platform's insets in grid
  pixels, at least 4 pixels from each edge);
- `layout.anchor(where, w, h, dx, dy)` places a box at one of nine anchors of the safe box;
- `layout.pointer()` gives the pointer in grid pixels; `layout.hover(box)` and `layout.clicked(box)`
  test it; `layout.claim(box)` claims a box against the touch controls for this frame.

## Alternatives considered

- **Drawing frames with glyphs of a box font:** no curves, no images, no clipping; ornament would
  stay crude.
- **An HTML overlay on the web build:** two UIs to keep alike, nothing on desktop or in XR.
- **A third-party immediate-mode GUI:** its look and input model would fight the pixel grid and the
  gamepad and touch navigation the game already has; the layer needed is small.
