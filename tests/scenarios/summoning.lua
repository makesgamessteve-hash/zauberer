-- SPDX-License-Identifier: MIT
-- Summoning in the game: with no water near, "," cannot form a water
-- elemental and calls a wisp instead; by a pool it forms one, bound and
-- following; its upkeep is paid from the mage's water mana, and unpaid its
-- binding weakens until it fades; "-" dismisses; ";" bonds a fox offered
-- meat and life mana, which follows and is kept with the game.
-- spec: summoning: Summoning in the game
scenario { name = "summoning" }
local game = demo.game
local pack = demo.pack
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded and sim.water_at(here).loaded then break end
  sleep(5)
end
demo.ecology.pause(game.ecology, true)
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
end
local function said(text)
  for _, line in ipairs(demo.flight_lines()) do
    if line:find(text, 1, true) then return true end
  end
  return false
end
local c = game.carpet
c.speed = 0
c.pos.alt = world.ground_height(c.pos) + 1
step(2)
press("mount")
local w = game.walker
local mage = game.magic.caster.mage
local sm = game.summoning
-- No water near: a wisp.
mage.mana.water, mage.mana.light = 40, 20
press("summon")
expect(#sm.bound == 1 and sm.bound[1].summons == "wisp" and said("Summoned: Wisp of light"), "a wisp")
press("dismiss")
expect(#sm.bound == 0 and said("Dismissed 1"))
-- By a pool, a water elemental.
local pool = world.move(w.pos, w.heading, 8, 0)
world.deform(pool, 3, -1.5)
sim.add_water(pool, 3, 25)
step(60)
mage.mana.water = 40
press("summon")
expect(#sm.bound == 1 and sm.bound[1].summons == "water_elemental", "a water elemental")
local el = sm.bound[1].creature
expect(world.distance(sim.creature(el).pos, pool) < 22 and sim.creature(el).bond, "formed from the pool, bound")
-- It follows the mage.
w.pos = world.move(w.pos, w.heading + math.pi, 25, 0)
w.pos.alt = world.ground_height(w.pos)
for _ = 1, 60 * 30 do
  step(1)
  mage.mana.water = math.max(mage.mana.water, 5)
  if world.distance(sim.creature(el).pos, w.pos) < 8 then break end
end
expect(world.distance(sim.creature(el).pos, w.pos) < 8, "it follows")
-- Unpaid, its binding weakens and it fades.
mage.mana.water = 0
for _ = 1, 60 * 60 do
  step(1)
  mage.mana.water = 0
  if #sm.bound == 0 then break end
end
expect(#sm.bound == 0 and sim.creature(el) == nil and said("fades"), "it faded")
-- A familiar: a fox offered meat and life mana.
local fox = sim.spawn_creature("fox", world.move(w.pos, w.heading, 4, 0))
press("bond")
expect(said("it wants raw meat"), "an offering is wanted")
pack.add(game.pack, "raw_meat", 1, 0)
mage.mana.life = 20
press("bond")
expect(sm.familiar and sm.familiar.creature == fox and said("A fox is your familiar"), "bonded")
step(60 * 10)
expect(world.distance(sim.creature(fox).pos, w.pos) < 6, "the fox keeps close")
expect(said("Familiar: fox (keen nose 1)"))
local back = demo.summoning.new(demo.summoning.snapshot(sm))
expect(back.familiar and back.familiar.species == "fox" and #back.bound == 0, "kept with the game")
demo.ecology.pause(game.ecology, false)
