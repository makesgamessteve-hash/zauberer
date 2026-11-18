-- SPDX-License-Identifier: MIT
-- The M3 exit demo, headless: lightning sets the forest by the carpet alight in
-- a west wind, the fire runs downwind and never onto water, rain puts it out,
-- a month away and the scar is greening again, and a dam breached on the
-- hillside sends its lake down the slope. The water balance holds throughout.
-- spec: simulation-tiers: The M3 exit demo
scenario { name = "living physics" }
world.view_distance(768, 3000) -- smaller rings keep the headless run quick
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded and world.chunk_stage(here) == 3 then break end
  sleep(5)
end
expect(sim.fire_at(here).loaded, "the carpet's ground is in the active tier")
local function balanced()
  local s = sim.water_stats()
  local given = s.rained + s.added
  return math.abs(s.on_ground + s.soaked + s.drained + s.left + s.removed - given) <= 1e-3 * math.max(given, 1)
end
-- 1. The richest dry fuel near the carpet, and a dry storm from the west.
local spot, best = nil, 0
for r = 0, 120, 6 do
  for a = 0, 5.9, 0.785 do
    local p = world.move(here, a, r, 0)
    local c = sim.fire_at(p)
    if c.loaded and not c.water and c.moisture < 0.25 and c.fuel > best then
      spot, best = p, c.fuel
    end
  end
end
expect(spot and best > 0.2, "dry fuel near the carpet")
sim.set_weather({ wind_east = 8, wind_north = 0, rain = 0, snow = 0, cloud = 1, storm = 1 })
expect(sim.lightning(spot) > 0, "lightning set it alight")
step(60 * 30)
-- 2. Downwind (east) more than upwind, and nothing burns on water.
local function scorched(p)
  local c = sim.fire_at(p)
  return c.burning or c.burnt
end
local down, up = 0, 0
for r = 8, 40, 4 do
  for side = -8, 8, 4 do
    down = down + (scorched(world.move(spot, math.pi / 2, r, side)) and 1 or 0)
    up = up + (scorched(world.move(spot, -math.pi / 2, r, side)) and 1 or 0)
  end
end
expect(down > up, "the fire ran downwind: " .. down .. " cells east, " .. up .. " west")
for r = 0, 60, 4 do
  for a = 0, 5.9, 0.4 do
    local c = sim.fire_at(world.move(spot, a, r, 0))
    expect(not (c.water and c.burning), "fire never burns on water")
  end
end
-- 3. Rain puts it out.
sim.set_weather({ wind_east = 2, rain = 80, storm = 0 })
for _ = 1, 180 do
  step(60)
  if sim.fire_stats().burning == 0 then break end
end
expect_eq(sim.fire_stats().burning, 0, "the rain put it out")
expect(balanced(), "the water balance holds")
-- 4. A month away: the scar is greening. Remember cells that burnt out.
local scars = {}
for r = 0, 60, 2 do
  for a = 0, 5.9, 0.3 do
    local p = world.move(spot, a, r, 0)
    if #scars < 30 and sim.fire_at(p).burnt then
      scars[#scars + 1] = p
    end
  end
end
expect(#scars > 0, "cells burnt out")
sim.natural_weather()
game.carpet.pos = world.move(here, 0, 1500, 0)
for _ = 1, 600 do
  step(10)
  if not sim.fire_at(spot).loaded then break end
end
expect(not sim.fire_at(spot).loaded, "the scar left the active tier")
world.set_time(world.time().time + 30 * 86400)
game.carpet.pos = here
for _ = 1, 900 do
  step(10)
  if sim.fire_at(spot).loaded and world.chunk_stage(spot) == 3 then break end
end
local ground, taller = 0, 0
for _, p in ipairs(scars) do
  local c = sim.fire_at(p)
  expect(c.loaded and c.burnt and not c.burning, "the burn scar is still there")
  expect(world.ground_surface(p) ~= "ash", "the ash is gone")
end
for _, plant in ipairs(world.plants(spot, 80)) do
  if sim.fire_at(plant.pos).burnt then
    if plant.layer == "ground" then ground = ground + 1 else taller = taller + 1 end
  end
end
expect(ground > 0, "ground cover is back on the scar")
expect_eq(taller, 0, "no shrubs or trees on it yet")
-- 5. A ring dam on a hillside near the carpet holds a pond; breached on the
-- downhill side, the pond runs down the slope.
local top, heading, drop
for r = 20, 200, 10 do
  for a = 0, 5.9, 0.785 do
    local p = world.move(here, a, r, 0)
    local w = sim.water_at(p)
    for h = 0, 5.9, 0.785 do
      local q = world.move(p, h, 40, 0)
      local d = world.ground_height(p) - world.ground_height(q)
      local below = sim.water_at(q)
      if w.loaded and not w.fixed and below.loaded and not below.fixed and d > 1.5 and d < 12
        and (not drop or d > drop) then
        top, heading, drop = p, h, d
      end
    end
  end
end
expect(top, "a hillside near the carpet")
for k = 0, 11 do
  world.deform(world.move(top, k * math.pi / 6, 14, 0), 6, 4)
end
local below = world.move(top, heading, 34, 0)
for _ = 1, 6 do
  sim.add_water(top, 8, 100)
  step(60 * 3)
end
step(60 * 12)
expect(sim.water_at(top).depth > 0.3, "a pond inside the ring")
expect(sim.water_at(below).depth < 0.01, "the dam holds")
expect(world.deform(world.move(top, heading, 14, 0), 6, -6) >= 1, "breached")
local wet = false
for _ = 1, 120 do
  step(60)
  if sim.water_at(below).depth > 0.01 then
    wet = true
    break
  end
end
expect(wet, "the pond ran down the slope")
expect(balanced(), "the water balance holds")
