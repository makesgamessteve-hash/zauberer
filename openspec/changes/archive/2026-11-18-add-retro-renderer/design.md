# Design: Retro Renderer

## Context

M0 renders into an internal framebuffer (default 640 × 360) and blits it to the window
with nearest-neighbour filtering. add-art-generators produced the palette, a 32³ lookup
table (`palettes/base_lut.png`, 1024 × 32: x = blue × 32 + red, y = green), the 8 × 8
Bayer matrix and the terrain material atlas (12 layers of 64 × 64, stacked vertically).

## Goals / Non-Goals

**Goals:** the retro look on desktop and WebGL 2 with identical shaders; terrain from the
streamed chunks to the far ring with bounded draw calls, triangles and CPU time;
curvature without precision loss.

**Non-Goals:** vegetation, clouds, time of day and weather tints (M2–M3), shadow maps,
sharp-bilinear upscaling, PS1/N64 filter toggles (later options).

## Decisions

### 1. Passes

Per view: sky (fullscreen triangle from `gl_VertexID`, depth test off), terrain, models,
sprites into the scene framebuffer; then one post pass converts the scene into the post
framebuffer (palette or plain copy); text is drawn into the post framebuffer; the post
framebuffer is blitted to the window and is what `render.capture("internal")` reads. Text
therefore keeps its exact colours.

### 2. Palette and dither

`c' = c + (bayer(x, y) − 0.5) × dither`, then each channel is rounded to one of 32 levels
and the LUT gives the palette colour. Everything uses `texelFetch` on integer coordinates,
so the result depends only on the scene colour: the GPU and the CPU reference
(`zb_palette_quantize`) agree exactly for the same input. The default amplitude, 0.08,
is about half the step between neighbouring colours of a ramp, so smooth gradients
dither between neighbours instead of banding.

### 3. Sky and fog

Sky: the view ray's elevation above the horizon (plus the dip `acos(R / (R + h))` at
altitude h) is mapped to a horizon-to-zenith gradient (`t(2 − t)` over 0.6 rad), with a
sun glow (`max(0, cos)^16 × 0.25`) and a disc of about 2°. Fog: `1 − exp(−(d k)²)` with
the horizon colour; `render.atmosphere{fog_distance = D}` picks `k` so that fog reaches
98 % at D. The engine updates the sky's up direction and dip from the camera's planet
position every frame. C reference functions (`zb_sky_color`, `zb_fog_factor`) document
and test the formulas.

### 4. Terrain levels of detail

| Level | Grid | Vertices per chunk | Used below (default) | Source |
| --- | --- | --- | --- | --- |
| 0 | 4 m | 65² | 600 m | stage 1 heights, stage 2 materials, atlas texture |
| 1 | 8 m | 33² | 1,600 m | stage 1 heights, untextured layer colours |
| 2 | 16 m | 17² | 4,000 m | stage 0 grid |
| 3 | 32 m | 9² | 8,000 m | stage 0 grid |
| 4 | 64 m | 5² | beyond | stage 0 grid |

A chunk without a heightfield uses level 2 at most. Vertices are placed in the chunk's own
frame (`zb_origin` at the chunk centre), computed in double precision; the draw transform
from that frame to the render origin (`zb_origin_transform`) carries the curvature
exactly and is recomputed only when the origin moves. Skirts (every edge vertex repeated
2 × grid + 8 m lower, quads in both windings) hide cracks between levels. Chunks whose
lowest point is under 0.5 m get a 5 × 5 water surface at sea level. Untextured vertices
carry the material's average atlas colour; the colour's alpha holds the atlas layer
(255 = untextured), read as a flat varying.

Meshes are rebuilt when a chunk's data version or level changes, nearest first, within
per-frame budgets (4 fine, 48 coarse); a chunk keeps drawing its old mesh until the new
one exists. Meshes of chunks the stream no longer holds are released.

Measured with the desktop profile at 700 m altitude (14,300 chunks): 3,755 draw items and
758,000 triangles after frustum culling, 1.5 ms of CPU per frame for the terrain view.
Merging far chunks into larger tiles would cut draw items further; it is left for the
horizon ring in M2.

### 5. Floating origin

`zb_world` owns the render origin and moves it to the first focus's chunk when the focus
is more than 1,024 m away horizontally. Scripts keep positions as planet positions and
convert with `world.to_local` each frame, so a move needs no work from them.

## Risks / Trade-offs

- [Per-chunk draws are many on the web] → The web profile's rings hold about a third of
  the desktop's chunks; tiles come with the horizon ring.
- [Palette mixing with fog produces unexpected hues] → The palette has dedicated sky and
  haze ramps; per-biome palettes come in M2.
