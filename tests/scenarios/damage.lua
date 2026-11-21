-- SPDX-License-Identifier: MIT
-- The damage model in play: fire under the carpet hurts, a ward of Fire turns
-- most of it aside, healing mends, a weave going off in the hand hurts; a
-- Fireball striking a deer burns it, withering hurts creatures and healing
-- mends them; at no health the player is defeated and gets up again where the
-- carpet first set out, with the HUD telling it all.
-- spec: damage-model: Harm to the player
-- spec: damage-model: Magic against creatures
-- spec: damage-model: Defeat
-- spec: damage-model: Shelter and warnings
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
-- A deer that stands its ground (a real one may bolt from the carpet first).
local calm = { id = "calm_deer", length = 1.9, mass = 120, walk = 1.4, run = 12, diet = "herbivore", sight = 110,
  hearing = 180, smell = 150, health = 70, aggression = 0, fear_of_fire = 0, fear_of_player = 0 }
sim.define_species(calm)
local deer = sim.spawn_creature("calm_deer", target)
local cr = sim.creature(deer)
expect(cr.alive)
local mana_before = game.magic.caster.mage.mana.fire
for _ = 1, 3 do -- a misfire (late, stray, weak) is cast again, as a player would
  game.magic.caster.mage.mana.fire = math.max(game.magic.caster.mage.mana.fire, 30)
  input.inject("cast_left", 1, 0, 40) -- the left hand holds Fireball
  step(42)
  for _ = 1, 60 * 4 do
    step(1)
    if sim.creature(deer).health < 70 then break end
  end
  if sim.creature(deer).health < 70 then break end
end
cr = sim.creature(deer)
expect(game.magic.caster.mage.mana.fire < math.max(mana_before, 30), "a Fireball was cast")
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
-- A hunter near a place keeps a defeated player from getting up there; a
-- defeated player is spared by the hunters near (and bad radii are refused).
local fauna = require("life.fauna")
local den = sim.creature(wolf).pos
expect(fauna.hunter_near(den, 10) and not fauna.hunter_near(world.move(den, 0, 2000, 0), 10))
local clear, moved_off = health.safe_spot(den, fauna.hunter_near)
expect(moved_off and world.distance(clear, den) > 39, "up away from the wolf")
expect(type(sim.spare_player(60, 300)) == "number")
expect(not pcall(sim.spare_player, -1, 10) and not pcall(sim.spare_player, 10, 4000))
sim.remove_creature(wolf)
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
-- Harm on foot: a hard landing, bites (the news said once), healing and
-- withering heard close by, and thick smoke.
local foot = health.new()
local spot = world.move(game.carpet.pos, 0, 0, 0)
spot.alt = world.ground_height(spot)
health.tick(foot, 1 / 60, { pos = spot, on_foot = true, landing = health.FALL_SAFE + 2 })
expect_eq(foot.hp, 100 - 2 * health.FALL_HARM, "a hard landing")
local bites = { { kind = "attacked" } }
health.tick(foot, 1 / 60, { pos = spot, on_foot = true, events = bites })
health.tick(foot, 1 / 60, { pos = spot, on_foot = true, events = bites })
expect_eq(foot.hp, 100 - 2 * health.FALL_HARM - 2 * health.BITE, "bitten twice")
local said = 0
for _, n in ipairs(foot.news) do said = said + (n.text == "Bitten!" and 1 or 0) end
expect_eq(said, 1, "the same news once")
health.tick(foot, 1 / 60, { pos = spot, on_foot = true, events = bites })
health.tick(foot, 1 / 60, { pos = game.carpet.pos, on_foot = false, events = bites }) -- the carpet is out of reach
expect_eq(foot.hp, 100 - 2 * health.FALL_HARM - 3 * health.BITE)
local before_heard = foot.hp
health.tick(foot, 1 / 60, { pos = spot, heard = { { kind = "heal", at = spot, amount = 10, radius = 4 } } })
expect_eq(foot.hp, before_heard + 10, "healed by a weave nearby")
health.tick(foot, 1 / 60, { pos = spot, heard = { { kind = "wither", at = spot, amount = 4 },
  { kind = "heal", at = world.move(spot, 0, 50, 0), amount = 10 } } })
expect_eq(foot.hp, before_heard + 6, "withered; the far healing missed")
for _ = 1, 20 do sim.emit(spot, "smoke", 50) end
step(10)
local choked = foot.hp
health.tick(foot, 1, { pos = spot, on_foot = true })
expect(foot.hp < choked and (foot.taken.smoke or 0) > 0, "choking smoke: " .. tostring(sim.gas_at(spot).smoke))
-- Below ground (a cellar, a cave) the surface's smoke and fire do not reach.
local cellar = world.move(spot, 0, 0, 0)
cellar.alt = spot.alt - 3
local sheltered = health.new()
health.tick(sheltered, 1, { pos = cellar, on_foot = true })
expect_eq(sheltered.hp, 100, "sheltered from the smoke below ground")
-- Standing in flames the HUD says to get out; a defeated player gets up
-- away from a fire burning where the carpet set out.
local field = world.move(spot, 0, 90, 0)
field.alt = world.ground_height(field)
expect(sim.ignite(field, 6) > 0, "the grass caught")
step(5)
expect(health.fire_near(field, 10))
local below = world.move(field, 0, 0, 0)
below.alt = field.alt - 3
local under = health.new()
health.tick(under, 0.5, { pos = below, on_foot = true })
expect_eq(under.hp, 100, "the fire above does not reach the cellar")
local burning = health.new()
health.tick(burning, 0.1, { pos = field, on_foot = true })
local told = false
for _, n in ipairs(burning.news) do told = told or n.text:find("get out of the fire", 1, true) ~= nil end
expect(burning.hp < 100 and told, "burning, and told to get out")
local up, moved = health.safe_spot(field)
expect(moved and world.distance(up, field) > 39 and not health.fire_near(up, health.SAFE), "up away from the fire")
local far = world.move(field, 0, 2000, 0)
local stay, stayed = health.safe_spot(far)
expect(not stayed and world.distance(stay, far) < 0.01, "nothing near: where the carpet set out")
game.home.pos = world.move(field, 0, 0, 0)
game.home.pos.alt = field.alt + 20
health.hurt(h, "magic", 1000)
step(2)
step(60 * 4)
expect(not h.down, "got up again")
expect(world.distance(game.carpet.pos, field) > 39, "away from the fire")
expect(math.abs(game.carpet.pos.alt - world.ground_height(game.carpet.pos) - 20) < 1, "as high as the carpet set out")
expect(hud_has("away from the danger"))
