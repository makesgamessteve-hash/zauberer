-- SPDX-License-Identifier: MIT
-- Fauna behaviour in the game: a hungry wolf pack downwind of a deer herd
-- picks up the scent, follows it upwind and makes a kill, which the HUD
-- tells; the herd stampedes; thirsty deer drink at a pool; a wolf downwind
-- of the mage on foot smells the mage; needs can be set from scripts.
-- spec: fauna-behaviour: Fauna in the game
scenario { name = "fauna" }
local game = demo.game
local fauna = demo.fauna
local ecology = demo.ecology
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.water_at(here).loaded and sim.wind_at(here).loaded then break end
  sleep(5)
end
ecology.pause(game.ecology, true) -- only the animals placed here
for _, id in ipairs(game.fauna.ids) do sim.remove_creature(id) end
expect(fauna.species().wolf.activity == "twilight" and fauna.species().crane.activity == "day")
-- Dusk here, and a breeze from the herd to the pack.
local t = world.time().time
for hour = 1, 30 do
  world.set_time(t + hour * 3600)
  local light = world.sky(here).daylight
  if light > 0.05 and light < 0.5 then break end
end
local field = world.move(here, 0, 0, 0)
local herd_at, pack_at = world.move(field, 0.6, 60, 0), world.move(field, 0.6 + math.pi, 70, 0)
local hp, pp = world.to_local(herd_at), world.to_local(pack_at)
local east, north = pp[1] - hp[1], -(pp[3] - hp[3])
local len = math.sqrt(east * east + north * north)
sim.set_weather({ cloud = 0, rain = 0, snow = 0, storm = 0, wind_east = 4 * east / len, wind_north = 4 * north / len,
  temperature = 14 }, 3600)
local herd, pack = {}, {}
for k = 1, 4 do
  herd[k] = sim.spawn_creature("deer", world.move(herd_at, k * 1.5, 4 + k * 3, 0))
  sim.set_needs(herd[k], { hunger = 0, thirst = 0, energy = 1 })
end
step(60 * 8) -- the herd's scent drifts downwind
for k = 1, 3 do
  pack[k] = sim.spawn_creature("wolf", world.move(pack_at, k * 2, 3 + k * 2, 0))
  sim.set_needs(pack[k], { hunger = 1, thirst = 0, energy = 1 })
end
local function gap()
  local best = math.huge
  for _, w in ipairs(pack) do
    for _, d in ipairs(herd) do
      local a, b = sim.creature(w), sim.creature(d)
      if a and b and b.alive then best = math.min(best, world.distance(a.pos, b.pos)) end
    end
  end
  return best
end
local start = gap()
local tracked, hunted, stampede, kill = false, false, 0, nil
for _ = 1, 60 * 180 do
  step(1)
  for _, w in ipairs(pack) do
    local c = sim.creature(w)
    tracked = tracked or c.action == "investigate"
    hunted = hunted or (c.action == "hunt" and c.target ~= nil)
  end
  local running = 0
  for _, d in ipairs(herd) do running = running + (sim.creature(d).action == "flee" and 1 or 0) end
  stampede = math.max(stampede, running)
  for _, ev in ipairs(game.fauna.recent) do
    if ev.kind == "died" and ev.by_species == "wolf" then kill = ev end
  end
  if kill then break end
end
expect(tracked, "the pack followed the scent")
expect(hunted, "and hunted")
expect(stampede >= 3, "the herd ran together: " .. stampede)
expect(kill and kill.species == "deer" and kill.cause == "impact", "a kill: " .. tostring(start))
local told = false
for _, line in ipairs(fauna.hud_lines(game.fauna)) do
  told = told or line:find("grey wolf killed a red deer", 1, true) ~= nil
end
expect(told, "the kill is news")
step(60 * 5)
local feeding = 0
for _, w in ipairs(pack) do feeding = feeding + (sim.creature(w).action == "feed" and 1 or 0) end
expect(feeding >= 1 and sim.creature(kill.id).eaten > 0, "they eat")
for _, id in ipairs(herd) do sim.remove_creature(id) end
for _, id in ipairs(pack) do sim.remove_creature(id) end
-- Thirsty deer go to a pool and drink.
local pool = world.move(field, 2.5, 40, 0)
world.deform(pool, 6, -2)
sim.add_water(pool, 5, 60)
step(60)
local thirsty = sim.spawn_creature("deer", world.move(pool, 0, 18, 0))
sim.set_needs(thirsty, { hunger = 0, thirst = 1, energy = 1 })
local drank = false
for _ = 1, 60 * 40 do
  step(1)
  local c = sim.creature(thirsty)
  drank = drank or (c.action == "drink" and c.thirst < 0.8)
  if drank then break end
end
expect(drank, "drank at the pool")
sim.remove_creature(thirsty)
-- On foot, the mage is smelt downwind.
local c = game.carpet
c.speed = 0
c.pos.alt = world.ground_height(c.pos) + 1
step(1)
input.inject("mount", 1, 0, 1)
step(3)
expect(game.walker ~= nil, "on foot")
local me = game.walker.pos
local downwind = world.move(me, 2.4, 60, 0) -- the breeze is set from the mage to it below
local dp, mp = world.to_local(downwind), world.to_local(me)
local de, dn = dp[1] - mp[1], -(dp[3] - mp[3])
local dl = math.sqrt(de * de + dn * dn)
sim.set_weather({ cloud = 0, rain = 0, snow = 0, storm = 0, wind_east = 4 * de / dl, wind_north = 4 * dn / dl,
  temperature = 14 }, 3600)
step(60 * 10)
local nose = sim.spawn_creature("wolf", downwind)
sim.set_needs(nose, { hunger = 1, thirst = 0, energy = 1 })
local smelt = false
for _ = 1, 60 * 20 do
  step(1)
  local w = sim.creature(nose)
  smelt = smelt or w.action == "investigate" or w.action == "hunt"
  if smelt then break end
end
expect(smelt, "the wolf smelt the mage")
-- Needs from scripts.
expect(not sim.set_needs(999999, { hunger = 0 }))
expect(not pcall(sim.set_needs, nose, { thirst = 2 }), "out of range")
expect(not pcall(sim.define_species, { id = "x", walk = 1, run = 2, activity = "always" }), "no such activity")
expect(not pcall(sim.define_species, { id = "x", walk = 1, run = 2, thirst = 1 }), "too thirsty")
ecology.pause(game.ecology, false)
