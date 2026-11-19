-- SPDX-License-Identifier: MIT
-- Alchemy in the game: in the tent's laboratory the HUD shows the chosen
-- recipe; "." chooses the next and K brews it: herbs, water and life mana
-- become healing draughts while the hours pass; drunk, a draught mends.
-- spec: alchemy-lab: Alchemy in the game
scenario { name = "alchemy" }
local game = demo.game
local pack = demo.pack
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded then break end
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
expect(game.tent.inside, "inside")
for _ = 1, 8 do
  if game.home_place.room ~= "laboratory" then
    input.inject("steer", 1, 0, 3)
    step(5)
  end
end
expect(game.home_place.room == "laboratory", "in the laboratory")
pack.add(game.pack, "healing_herbs", 10, 0)
pack.add(game.pack, "boiled_water", 1, 0)
local mage = game.magic.caster.mage
mage.mana.life = 30
press("cloak_colour")
press("cloak_colour")
press("cloak_colour")
press("cloak_colour") -- (round the four recipes, back to the first)
local before = world.time().time
press("cook")
expect(pack.count(game.pack, "healing_draught") >= 2 and said("Brewed"), "draughts brewed")
expect(world.time().time - before >= 4 * 3600, "the hours passed")
expect(game.known.healing_draught and game.known.life_essence, "what was in the vessel is known")
press("cook")
expect(said("not enough"), "nothing left to brew from")
-- Drunk, a draught mends.
game.health.hp = 40
game.survival.water = 30
press("drink")
expect(game.health.hp >= 75, "mended: " .. game.health.hp)
