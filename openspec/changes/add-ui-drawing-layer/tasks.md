# Tasks

- [ ] 1.1 The UI pass in the renderer: the grid from the internal resolution and window, an orthographic pass after post-processing, the safe area
- [ ] 1.2 The batcher: rectangles, images and atlas cells, nine-slice tiling, text with shadow and outline, clip rectangles; the null backend records the calls
- [ ] 1.3 Textures made by scripts: make, update a region, free; limits and errors
- [ ] 1.4 The script functions (`render.ui_*`, `render.texture_from_pixels`, `render.update_texture`, `render.free_texture`) with API docs
- [ ] 1.5 `ui/layout.lua`: grid, safe box, anchors, stacks, hit testing for mouse and touch; a tapped box takes the touch from the flight controls
- [ ] 1.6 WebXR: the UI grid on the floating HUD panel
- [ ] 1.7 Tests: unit tests of the grid, nine-slice tiling, batching and clipping (C); a scenario per spec scenario; a rendered golden of a framed panel
