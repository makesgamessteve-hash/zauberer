# Tasks

- [ ] 1.1 The renderer: a white texture, `zb_render_draw_quad` into the text path's vertices and batches (merging batches of one texture), the clip stack with CPU clipping, nine-slice tiling, text shadows
- [ ] 1.2 Textures made by scripts: make, update a region, free; limits and errors
- [ ] 1.3 The platform normalises mouse positions; the input keeps the pointer (mouse or last touch), presses, releases and the wheel, and the UI's claimed rectangles against the touch controls
- [ ] 1.4 The script functions (`render.ui_grid`, `render.ui_rect`, `render.ui_image`, `render.ui_nine`, `render.ui_clip`, `render.ui_unclip`, `render.texture_from_pixels`, `render.update_texture`, `render.free_texture`, `input.pointer`, `input.claim`) with API docs
- [ ] 1.5 `ui/layout.lua`: grid, safe box, anchors, the pointer in grid pixels, hover, click and claim
- [ ] 1.6 Tests: C unit tests of quads, batching, nine-slice tiling, clipping, script textures and the pointer; a scenario per spec scenario; a rendered golden of a framed panel
