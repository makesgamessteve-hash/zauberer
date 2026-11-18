-- SPDX-License-Identifier: MIT
-- Rendered (window, 640 x 360): streamed terrain with sky, fog and the palette.
-- spec: retro-rendering: Terrain scene
-- spec: retro-rendering: Palette colours only
-- spec: retro-rendering: Palette off
scenario { name = "terrain view" }
demo.game.set_mode("showcase")
demo.game.hud = false
world.view_distance(768, 6000)
local peak = world.position(2, 785, 11510, 128, 128, 0)
peak.alt = world.height(peak)
local base = world.position(2, 773, 11510, 128, 128, 0)
local cam = world.position(base.face, base.x, base.y, base.lx, base.ly, world.height(base) + 500)
world.set_focus(1, cam, { 0, 0, 0 }, { 1, 0, 0 })
render.atmosphere { zenith = { 0.22, 0.42, 0.78 }, horizon = { 0.72, 0.8, 0.88 }, sun = { 0.5, 0.35, -0.8 },
  fog_distance = 6000 }
render.palette {}
demo.game.camera = function()
  return { eye = world.to_local(cam), target = world.to_local(peak), up = world.up_at(cam), fov = 60, near = 1,
    far = 20000 }
end
for _ = 1, 3000 do
  step(1)
  local st = world.stream_stats()
  if st.queued == 0 and st.jobs == 0 and st.stage2 + st.stage3 > 0 then break end
end
render.pin_time(10) -- water and plants hold still for the golden image
step(120) -- terrain meshes are built over a few frames
expect(render.stats().terrain_items > 100, "terrain chunks drawn")
local ok, info = render.match_golden("terrain")
expect(ok, info)
render.pin_time()
expect_error(function() render.pin_time(-1) end, "time must not be negative")

-- Every pixel is one of the palette's colours.
local palette = {}
for _, hex in ipairs(load(vfs.read("palettes/base.lua"))().hex) do
  palette[tonumber(hex, 16)] = true
end
local function off_palette()
  local size = render.capture("internal")
  local count = 0
  for y = 0, size.height - 1, 3 do
    for x = 0, size.width - 1, 3 do
      local r, g, b = render.pixel(x, y)
      if not palette[(r << 16) | (g << 8) | b] then
        count = count + 1
      end
    end
  end
  return count
end
expect_eq(off_palette(), 0, "pixels outside the palette")
render.palette(false)
step(2)
expect(off_palette() > 1000, "without the palette the scene keeps its own colours")
