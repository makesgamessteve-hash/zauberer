-- SPDX-License-Identifier: MIT
-- The workshop in the game: in the tent's workshop "." chooses a work and K
-- does it: cord from fibres, a horn lantern, then the lantern enchanted
-- with light; outside, the lantern stays alight a whole day and night on
-- the land's mana alone (the mage has none to give); the bench also mends
-- the carpet fully.
-- spec: artificing: The workshop in the game
scenario { name = "workshop" }
local game = demo.game
local pack = demo.pack
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded and mana.at(here).loaded then break end
  sleep(5)
end
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
end
local function said(text)
  for _, line in ipairs(demo.home.hud_lines(game.home_place)) do
    if line:find(text, 1, true) then return true end
  end
  return false
end
local function choose(name)
  for _ = 1, 20 do
    local h = game.home_place
    local nc = #demo.crafting.crafts()
    local w = h.work or 1
    local job = w <= nc and demo.crafting.crafts()[w] or demo.artificing.enchantments()[w - nc]
    if job.name == name then return true end
    press("cloak_colour")
  end
  return false
end
local c = game.carpet
c.speed = 0
c.pos.alt = world.ground_height(c.pos) + 1
step(2)
press("mount")
press("tent")
expect(game.tent, "pitched")
game.walker.pos = demo.tent.door(game.tent)
step(2)
press("mount")
for _ = 1, 8 do
  if game.home_place.room ~= "workshop" then
    input.inject("steer", 1, 0, 3)
    step(5)
  end
end
expect(game.home_place.room == "workshop", "in the workshop")
local p = game.pack
pack.add(p, "reed_fibre", 3, 0)
pack.add(p, "deadwood", 1, 0)
pack.add(p, "resin", 1, 0)
pack.add(p, "hide", 1, 0)
expect(choose("Cord"))
press("cook")
expect(pack.count(p, "cord") == 1 and said("Made cord 1"), "a cord")
expect(choose("Horn lantern"))
press("cook")
expect(pack.count(p, "lantern") == 1 and said("Made horn lantern 1"), "a lantern")
local mage = game.magic.caster.mage
mage.mana.light = 30
expect(choose("Lantern of perpetual light"))
press("cook")
expect(#game.artifacts == 1 and pack.count(p, "lantern") == 0 and said("Enchanted: Lantern of perpetual light"))
-- The bench mends the carpet fully.
game.condition.integrity = 40
pack.add(p, "cord", 2, 0)
expect(choose("Mend the carpet"))
press("cook")
expect(game.condition.integrity == 100, "mended")
-- Outside, a day and a night on the land's mana alone.
press("mount")
expect(not game.tent.inside)
mage.mana.light = 0
local dark = 0
for _ = 1, 360 do
  world.set_time(world.time().time + 240)
  step(1)
  mage.mana.light = 0
  if not demo.artificing.working(game.artifacts, "lantern_of_light") then dark = dark + 1 end
end
expect(dark == 0, "alight all day and night: dark " .. dark .. " ticks")
local found = false
for _, line in ipairs(demo.flight_lines()) do
  found = found or line:find("Lantern of perpetual light: alight", 1, true)
end
expect(found, "the HUD says so")
