-- SPDX-License-Identifier: MIT
-- Creatures from Lua: the base species live near the carpet's start, walkers
-- on the ground and the storm bird above it; deer flee a fire lit by them;
-- a storm bird struck down dies with its storm motes; the counts add up; the
-- player's whereabouts reach the creatures; bad arguments are refused.
-- spec: creatures: Creatures from Lua
scenario { name = "creatures" }
local game = demo.game
local fauna = demo.fauna
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.water_at(here).loaded then break end
  sleep(5)
end
step(30)
local f = game.fauna
expect_eq(#f.ids, 9, "five deer, three wolves and a storm bird")
local by_species = {}
for _, id in ipairs(f.ids) do
  local c = sim.creature(id)
  expect(c and c.alive, "alive")
  by_species[c.species] = (by_species[c.species] or 0) + 1
  local ground = world.ground_height(c.pos)
  if c.species == "storm_bird" then
    expect(c.pos.alt > ground + 10, "the bird flies: " .. c.pos.alt - ground)
  elseif not c.frozen then
    expect(math.abs(c.pos.alt - ground) < 0.6 or sim.water_at(c.pos).frozen, "on the ground")
  end
end
expect(by_species.deer == 5 and by_species.wolf == 3 and by_species.storm_bird == 1)
expect_eq(fauna.species().wolf.name, "Grey wolf")
-- A species of a script's own.
local index = sim.define_species({ id = "lynx", length = 1.2, mass = 80, walk = 1.2, run = 8, diet = "omnivore",
  sight = 50, hearing = 100, smell = 250, health = 80, resist = { impact = 0.3 }, color = { 0.3, 0.25, 0.2 } })
expect_eq(index, 9) -- after the nine base species (two of them summoned)
local spot = world.move(here, game.carpet.heading, 60, 0)
local lynx = sim.spawn_creature("lynx", spot)
step(2)
expect_eq(sim.creature(lynx).species, "lynx")
expect(sim.creature(lynx).hunger >= 0 and sim.creature(lynx).energy == 1)
local near = sim.creatures_near(sim.creature(lynx).pos, 5) -- within 5 m, heights counted
local found_lynx = false
for _, id in ipairs(near) do found_lynx = found_lynx or id == lynx end
expect(found_lynx, "found near where it stands")
-- Deer next to a fire flee from it.
local herd = {}
for k = 1, 3 do
  herd[k] = sim.spawn_creature("deer", world.move(spot, 1.5, 10 + k * 2, 0))
end
step(30)
local lit = sim.ignite(world.move(spot, 1.5, 6, 0), 3)
step(60)
local fled = 0
for _, id in ipairs(herd) do
  local c = sim.creature(id)
  fled = fled + ((c.action == "flee" and c.fear > 0.5) and 1 or 0)
end
expect(lit > 0 and fled >= 2, "deer flee the fire: " .. fled .. " of 3 (cells lit " .. lit .. ")")
sim.cool(spot, 40, 1e12)
-- A storm bird struck down: its death brings its storm motes.
local bird
for _, id in ipairs(f.ids) do
  if sim.creature(id).species == "storm_bird" then bird = id end
end
expect(not sim.hurt(bird, "impact", 1000), "dead")
step(1)
local died
for _, ev in ipairs(f.recent) do
  if ev.kind == "died" and ev.id == bird then died = ev end
end
expect(died and died.cause == "impact" and died.magical and died.mana == "storm" and died.motes == 40,
  "the bird's death and its motes")
local told = false
for _, line in ipairs(fauna.hud_lines(f)) do told = told or line:find("storm bird died", 1, true) ~= nil end
expect(told, "the death is news")
expect(not sim.creature(bird).alive)
-- Healing never goes past full health.
expect(sim.hurt(lynx, "magic", -500))
expect_eq(sim.creature(lynx).health, 80)
expect(sim.hurt(lynx, "impact", 10))
expect_eq(sim.creature(lynx).health, 73) -- resists 30 % of blows
-- Stimuli and the player.
sim.stimulus("threat", spot, 1, 2)
sim.set_player(here, true)
sim.set_player(nil)
step(15)
expect(sim.creature(lynx).fear > 0.5, "the threat frightens")
local stats = sim.creature_stats()
local total = stats.alive + stats.dead + stats.frozen
local wild = 0 -- the animals the ecology brought around the carpet
for _, chunk in pairs(game.ecology.live) do
  for _, ids in pairs(chunk) do wild = wild + #ids end
end
expect_eq(total, 9 + 1 + 3 + wild)
expect(stats.dead >= 1 and stats.actions.flee >= 1)
expect(sim.tier_stats().creatures_ms >= 0)
expect(sim.remove_creature(lynx) and not sim.remove_creature(lynx))
expect(sim.creature(lynx) == nil)
-- Bad arguments.
expect(not pcall(sim.spawn_creature, "unicorn", spot), "no such species")
expect(not pcall(sim.hurt, bird, "tickle", 1), "no such harm")
expect(not pcall(sim.hurt, bird, "magic", 1e9), "too much")
expect(not pcall(sim.stimulus, "smell", spot, 1, 1), "no such stimulus")
expect(not pcall(sim.stimulus, "noise", spot, 100, 1), "too loud")
expect(not pcall(sim.define_species, { id = "snail", walk = 2, run = 1 }), "runs slower than it walks")
for _, id in ipairs(herd) do sim.remove_creature(id) end
