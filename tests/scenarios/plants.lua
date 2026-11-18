-- SPDX-License-Identifier: MIT
-- M2 vegetation from Lua: the species registry, and plants around the carpet
-- that suit the place (headless; chunks near the carpet get their plants).
-- spec: vegetation: Plants from Lua
scenario { name = "plants" }
local game = demo.game
local flora = world.flora()
expect(#flora >= 19, "the base species")
local layers = {}
for _, s in ipairs(flora) do
  layers[s.layer] = (layers[s.layer] or 0) + 1
  expect(type(s.id) == "string" and type(s.name) == "string")
end
expect(layers.tree and layers.shrub and layers.ground, "trees, shrubs and ground cover")
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if world.chunk_stage(here) == 3 and #world.plants(here, 150) > 0 then break end
  sleep(5)
end
local plants = world.plants(here, 150)
expect(#plants > 0, "plants grow around the carpet")
local by_id = {}
for _, s in ipairs(flora) do by_id[s.id] = s end
for i = 1, math.min(#plants, 200) do
  local p = plants[i]
  expect(by_id[p.species], "a known species: " .. tostring(p.species))
  expect_eq(p.layer, by_id[p.species].layer)
  expect(p.height > 0.1 and p.height < 60, "a plausible height")
  expect(world.distance(here, p.pos) <= 150.5, "within the radius")
  expect(math.abs(p.pos.alt - world.ground_height(p.pos)) < 1.5, "standing on the ground")
end
expect_eq(#world.plants(here, 0), 0)
-- Every radius finds exactly the plants within it (chunks out of reach are
-- skipped and a box rules most out first: none is missed).
for _, probe in ipairs({ { 0, 0 }, { 0.7, 90 }, { 2.2, 140 }, { 4.1, 200 }, { 5.5, 250 } }) do
  local at = world.move(here, probe[1], probe[2], 0)
  local all = world.plants(at, 64)
  expect(#all < 4096, "under the cap")
  for _, r in ipairs({ 3, 20, 40 }) do
    local want = 0
    for _, pl in ipairs(all) do want = want + (world.distance(pl.pos, at) <= r and 1 or 0) end
    expect_eq(#world.plants(at, r), want, "plants within " .. r .. " m")
  end
end
expect_error(function() world.plants(here, 300) end, "radius must be 0..256 metres")
