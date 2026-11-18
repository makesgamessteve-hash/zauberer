-- SPDX-License-Identifier: MIT
-- Rendered: plants around the carpet are drawn as billboards and sway in the
-- wind (rendered tests).
-- spec: vegetation: Plants are drawn
scenario { name = "forest view" }
local game = demo.game
game.hud = false
for _ = 1, 900 do
  step(1)
  local s = world.stream_stats()
  if s.queued == 0 and s.jobs == 0 and render.stats().foliage_instances > 0 then break end
  sleep(5)
end
step(20)
local st = render.stats()
expect(st.foliage_items > 0 and st.foliage_instances > 100, "plants are drawn")
-- Look closely at the nearest tree: it fills part of the view and sways.
local tree
for _, p in ipairs(world.plants(game.carpet.pos, 250)) do
  if p.layer == "tree" then
    tree = p
    break
  end
end
expect(tree, "a tree nearby")
local base = tree.pos
local function at(dx, alt)
  return { face = base.face, x = base.x, y = base.y, lx = base.lx + dx, ly = base.ly, alt = base.alt + alt }
end
game.camera_override = function()
  return { eye = world.to_local(at(-tree.height * 2.2, tree.height * 0.6)),
    target = world.to_local(at(0, tree.height * 0.6)), up = { 0, 1, 0 }, fov = 50, near = 0.5, far = 3000 }
end
step(5)
local size = render.capture("internal")
local function sample()
  local px = {}
  for y = size.height // 4, size.height * 3 // 4, 3 do
    for x = size.width // 4, size.width * 3 // 4, 3 do
      local r, g, b = render.pixel(x, y)
      px[#px + 1] = r * 65536 + g * 256 + b
    end
  end
  return px
end
local first = sample()
step(40)
render.capture("internal")
local second, changed = sample(), 0
for i = 1, #first do
  changed = changed + (first[i] ~= second[i] and 1 or 0)
end
expect(changed > 0, "the tree sways in the wind")
