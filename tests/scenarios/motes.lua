-- SPDX-License-Identifier: MIT
-- Mana motes in play: a storm bird struck down releases its storm motes; the
-- carpet flying close draws them in and collects them, up to the mage's
-- capacity; motes left alone dissolve back into the field; a very rich place
-- blooms into motes; holding still channels mana from the field.
-- spec: mana-motes: Motes from the fallen
-- spec: mana-motes: Collecting and dissolving
-- spec: mana-motes: Blooms and channeling
scenario { name = "motes" }
local game = demo.game
local motes = demo.motes
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if mana.at(here).loaded then break end
  sleep(5)
end
local m = game.magic.caster.mage
local field = game.motes
-- The storm bird dies: its motes appear where it fell.
local bird
for _, id in ipairs(game.fauna.ids) do
  if sim.creature(id).species == "storm_bird" then bird = id end
end
local before = #field.list
expect(not sim.hurt(bird, "magic", 1000))
step(2)
expect_eq(#field.list, before + 8, "eight storm motes")
local fallen = sim.creature(bird).pos
for k = before + 1, #field.list do
  local mote = field.list[k]
  expect_eq(mote.color, "storm")
  expect(world.distance(mote.pos, fallen) < 4, "where it fell")
end
expect(field.released >= 40 - 1e-9)
-- Motes released by the carpet are drawn in and collected.
local function empty()
  for _, c in ipairs(require("magic.mage").COLORS) do m.mana[c] = 0 end
  field.list = {}
end
expect_eq(require("magic.mage").total(m), m.capacity, "a new game starts with a full reservoir")
empty()
local ahead = world.move(game.carpet.pos, game.carpet.heading, 10, 0)
expect_eq(motes.release(field, ahead, "storm", 30, 6), 6)
step(60 * 4)
expect(m.mana.storm > 25, "collected: " .. m.mana.storm)
expect(field.collected > 25)
-- A full mage collects only what fits; the rest stays.
empty()
m.mana.fire = m.capacity - 5
motes.release(field, ahead, "life", 20, 4)
step(60 * 2)
expect(math.abs(m.mana.life - 5) < 1e-6, "only what fits: " .. m.mana.life)
-- Left alone, motes dissolve into the field.
motes.LIFETIME = 3
local far = world.move(game.carpet.pos, game.carpet.heading + 3.1, 120, 0)
local stored = mana.at(far).stored.water
local dissolved = field.dissolved
motes.release(field, far, "water", 12, 3)
step(60 * 5)
expect(field.dissolved - dissolved > 11.9, "dissolved: " .. field.dissolved - dissolved)
expect(mana.at(far).stored.water > stored, "back in the field")
motes.LIFETIME = 120
-- A very rich place blooms into motes, taken from the field.
mana.release(far, "fire", 500)
local cell = mana.at(far)
expect(cell.stored.fire >= 0.9 * cell.cap.fire)
field.since_bloom = motes.BLOOM_EVERY
local n = motes.bloom(field, far, 1 / 60)
expect(n > 0, "a bloom")
-- Holding still channels mana from the field; flying fast does not.
m.mana.fire = 0
local gained = motes.channel(m, here, 1, 0)
expect(gained > 0 and gained <= m.channel + 1e-9, "channelled " .. gained)
expect_eq(motes.channel(m, here, 1, 20), 0)
expect(motes.release(field, here, "void", 0, 3) == 0, "nothing from nothing")
