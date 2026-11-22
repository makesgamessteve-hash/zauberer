-- SPDX-License-Identifier: MIT
-- Rendered (window, 640 x 360): the UI layer. A UI colour comes out exactly
-- (the palette never touches it); a nine-slice frame keeps its corners and
-- tiles its edges; clips cut a scrolled list; a painted texture is drawn and
-- updated; boxes sit at the safe box's anchors; a tap on a claimed box is the
-- UI's, not the climb stick's under it.
-- spec: ui-drawing: Window sizes
-- spec: ui-drawing: A framed panel
-- spec: ui-drawing: A scrolled list
-- spec: ui-drawing: A painted texture
-- spec: ui-drawing: Anchors and a tap
scenario { name = "ui panel" }
local game = demo.game
local layout = require("ui.layout")
game.hud = false
step(2)
local g = render.ui_grid()
expect_eq(g.width, 640)
expect_eq(g.height, 360)
expect(math.abs(g.w - 1) < 1e-6 and math.abs(g.h - 1) < 1e-6, "the grid fills the window")

-- A 16 x 16 frame cell: a red border 6 wide around a blue middle; a painted texture.
local function cell(border, edge, middle)
  local px = {}
  for y = 0, 15 do
    for x = 0, 15 do
      local e = x < border or y < border or x >= 16 - border or y >= 16 - border
      local c = e and edge or middle
      px[#px + 1] = string.char(c[1], c[2], c[3], 255)
    end
  end
  return table.concat(px)
end
local frame_tex = render.texture_from_pixels(16, 16, cell(6, { 200, 30, 40 }, { 30, 60, 160 }))
local paint = render.texture_from_pixels(64, 64, string.rep(string.char(240, 200, 60, 255), 64 * 64))
render.update_texture(paint, 0, 0, 4, 4, string.rep(string.char(10, 10, 10, 255), 16))
expect(not pcall(render.texture_from_pixels, 2048, 2048, ""), "too large is refused")
expect(not pcall(render.texture_from_pixels, 4, 4, "abc"), "too few bytes are refused")
expect(not pcall(render.update_texture, paint, 62, 62, 4, 4, string.rep("x", 64)), "outside is refused")
expect_eq(select(1, render.texture_size(paint)), 64)

game.ui_overlay = function()
  render.ui_rect(10, 10, 20, 20, { 0.2, 0.6, 0.4, 1 })
  render.ui_nine(frame_tex, 40, 40, 100, 40, { border = { 6, 6, 6, 6 } })
  render.ui_nine(frame_tex, 160, 40, 9, 9, { border = { 6, 6, 6, 6 } })
  render.ui_image(paint, 300, 10, { scale = 1 })
  render.ui_clip(200, 100, 50, 20)
  for i = 0, 19 do render.ui_rect(200, 100 + i * 5, 100, 4, { 1, 1, 1, 1 }) end
  render.ui_unclip()
  render.draw_text(demo.assets.font, "Framed", 40, 90, 1, { 1, 1, 1, 1 }, { 0, 0, 0, 1 })
end
step(3)
local size = render.capture("internal")
expect_eq(size.width, 640)
local function px(x, y) return { render.pixel(x, y) } end
local p = px(15, 15)
expect(p[1] == 51 and p[2] == 153 and p[3] == 102, string.format("the UI colour exactly: %d %d %d", p[1], p[2], p[3]))
p = px(41, 41)
expect(p[1] == 200 and p[3] == 40, "the frame's corner is whole")
p = px(90, 60)
expect(p[3] == 160 and p[1] == 30, "its middle is tiled from the cell's middle")
expect(px(90, 41)[1] == 200, "its top edge is the border")
expect(px(301, 11)[1] == 10, "the painted region shows")
p = px(310, 30)
expect(p[1] == 240 and p[3] == 60, "the rest of the painting too")
expect(px(220, 110)[1] == 255, "inside the clip")
expect(px(270, 110)[1] ~= 255, "cut at the clip's right")
expect(px(220, 130)[1] ~= 255, "cut at the clip's bottom")
expect(render.stats().ui_batches <= 8, "batched: " .. render.stats().ui_batches)

-- Anchors of the safe box, with a notch's inset at the top.
layout.set_insets(0, 10, 0, 0)
local s = layout.safe()
expect(s.x == 4 and s.y == 14 and s.w == 632 and s.h == 342, "the safe box")
local box = layout.anchor("bottom_right", 50, 20, 4, 4)
expect(box.x == 640 - 4 - 4 - 50 and box.y == 360 - 4 - 4 - 20, "4 pixels inside the corner")
local top = layout.anchor("top", 100, 10)
expect(top.y == 14 and top.x == 270, "below the notch, in the middle")
local rows = layout.stack(10, 10, "down", 2, { { 30, 8 }, { 30, 8 } })
expect(rows[2].y == 20, "stacked")
layout.set_insets()

-- A tap: first a touch elsewhere makes the screen a touch screen (its controls appear).
input.inject_pointer(0.5, 0.1, true, true)
step(2)
input.inject_pointer(0.5, 0.1, false, true)
step(2)
expect_eq(input.mode(), "touch")
demo.controls.setup_touch() -- (a real touch screen's controls appear through the platform's events)
local stick = { x = 0.86 * 640 - 20, y = 0.70 * 360 - 20, w = 40, h = 40 } -- over the climb stick
game.ui_overlay = function() layout.claim(stick) end
step(2)
input.inject_pointer(0.86, 0.70, true, true)
expect(layout.clicked(stick) and layout.hover(stick), "the box has the tap")
input.inject_pointer(0.86, 0.62, nil, true) -- (dragged, as a stick would be)
step(2)
expect(not input.state("climb_touch").held, "the climb stick under it does not move")
input.inject_pointer(0.86, 0.70, false, true)
step(2)
-- Without the claim the same tap moves the stick.
game.ui_overlay = nil
step(2)
input.inject_pointer(0.86, 0.70, true, true)
input.inject_pointer(0.86, 0.62, nil, true)
step(2)
expect(input.state("climb_touch").held, "unclaimed, the stick has it")
input.inject_pointer(0.86, 0.62, false, true)
step(2)
-- The mouse points too.
input.inject_pointer(0.25, 0.5)
local pt = layout.pointer()
expect(math.abs(pt.x - 160) < 0.5 and math.abs(pt.y - 180) < 0.5 and not pt.touch, "the mouse in grid pixels")
input.inject_pointer(0.25, 0.5, true)
expect(layout.clicked({ x = 150, y = 170, w = 20, h = 20 }), "a click")
input.inject_pointer(0.25, 0.5, false)
render.free_texture(paint)
render.free_texture(frame_tex)
