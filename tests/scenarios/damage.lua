-- SPDX-License-Identifier: MIT
-- The damage model in play: fire under the carpet hurts, a ward of Fire turns
-- most of it aside, healing mends, a weave going off in the hand hurts; a
-- Fireball striking a deer burns it, withering hurts creatures and healing
-- mends them; at no health the player is defeated and gets up again where the
-- carpet first set out, with the HUD telling it all.
-- spec: damage-model: Harm to the player
-- spec: damage-model: Magic against creatures
-- spec: damage-model: Defeat
scenario { name = "damage" }
local game = demo.game
local health = demo.health
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded then break end
  sleep(5)
end
local h = game.health
expect_eq(h.hp, 100)
local function hud_has(text)
  for _, line in ipairs(demo.flight_lines()) do
    if line:find(text, 1, true) then return true end
  end
  return false
end
expect(hud_has("Health 100/100"))
-- Down low over a fire: it burns.
local c = game.carpet
c.speed = 0
c.pos.alt = world.ground_height(c.pos) + 1
sim.ignite(c.pos, 4)
step(60)
local burnt = 100 - h.hp
expect(burnt > 5, "burnt: " .. burnt)
expect((h.taken.heat or 0) > 5)
-- A ward of Fire turns most of it aside.
local fx = game.magic.caster.effects
fx.wards.fire = fx.time + 60
local before = h.hp
step(60)
local warded = before - h.hp
expect(warded < burnt * 0.5, "warded: " .. warded .. " against " .. burnt)
fx.wards.fire = nil
sim.cool(c.pos, 20, 1e12)
c.pos.alt = c.pos.alt + 40
step(10)
-- Healing mends; no regeneration while harm is fresh.
expect(health.heal(h, 10) == 10 or h.hp == 100)
-- A weave going off in the hand.
before = h.hp
health.tick(h, 1 / 60, { cast = { ok = true, plan = { misfire = "backfire", power = 10 } } })
expect_eq(h.hp, before - 5)
expect(hud_has("went off in your hand"))
-- A Fireball strikes a deer in its path and burns it.
c.pos = world.move(c.pos, c.heading + 1.6, 60, 0) -- away from the burnt patch
c.pos.alt = world.ground_height(c.pos) + 2
step(1)
local target = world.move(c.pos, c.heading, 16, 0)
local deer = sim.spawn_creature("deer", target)
local cr = sim.creature(deer)
expect(cr.alive)
local mana_before = game.magic.caster.mage.mana.fire
input.inject("cast_left", 1, 0, 40) -- the left hand holds Fireball
step(40)
step(60)
cr = sim.creature(deer)
expect(game.magic.caster.mage.mana.fire < mana_before, "a Fireball was cast")
expect(cr.health < 70, "the deer was struck: " .. cr.health)
-- Withering hurts creatures, healing mends them.
local wolf = sim.spawn_creature("wolf", world.move(c.pos, 0, 30, 0))
local wolf_pos = sim.creature(wolf).pos
local effects = require("magic.effects")
effects.apply(fx, { weave = { verb = "unmake", colors = { "life" }, form = "circle" }, power = 10,
  extent = { radius = 4, duration = 1 } }, { origin = wolf_pos, target = wolf_pos })
local hurt = sim.creature(wolf).health
expect(hurt < 60, "withered: " .. hurt)
effects.apply(fx, { weave = { verb = "create", colors = { "life" }, form = "circle" }, power = 5,
  extent = { radius = 4, duration = 1 } }, { origin = wolf_pos, target = wolf_pos })
expect(sim.creature(wolf).health > hurt, "healed")
-- Defeat: at no health the player falls, and gets up again at home.
health.hurt(h, "magic", 1000)
step(2)
expect(h.down ~= nil and h.hp == 0, "defeated")
expect(hud_has("You are defeated"))
step(60 * 4)
expect(not h.down and h.hp == health.REVIVE, "got up again")
expect(world.distance(game.carpet.pos, game.home.pos) < 1, "back where the carpet set out")
expect_eq(h.defeats, 1)
-- Saved with the game.
local snap = health.snapshot(h)
local again = health.new(snap)
expect(again.hp == h.hp and again.defeats == 1)
expect(not pcall(health.hurt, h, "tickle", 1))
