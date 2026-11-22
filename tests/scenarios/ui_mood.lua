-- SPDX-License-Identifier: MIT
-- luacheck: globals render (ui_nine is wrapped to record the frames' colour)
-- The UI takes the mood of the world: frames tinted by the light (white at
-- noon, warm with the sun low, lapis at night, shaded in a storm; mana colours
-- untouched; high contrast untinted); the edges warm with a wolf near, a hit
-- marks its side, low health beats; the UI's sounds for a toast, a page turn
-- and a new goal.
-- spec: ui-mood: Dusk and night
-- spec: ui-mood: A wolf closes in
-- spec: ui-mood: A goal ticked
scenario { name = "ui mood" }
local game = demo.game
local mood = require("ui.mood")
local kit = require("ui.kit")
step(5)
local noon, dusk = mood.tint_for(1, 0, 0), mood.tint_for(0.45, 1, 0)
local night, storm = mood.tint_for(0.05, 0, 0), mood.tint_for(1, 0, 1)
expect(noon[1] == 1 and noon[2] == 1 and noon[3] == 1, "white at noon")
expect(dusk[1] > dusk[3], "warm with the sun low")
expect(night[3] > night[1], "lapis at night")
expect(storm[1] < 1 and storm[3] < 1, "shaded in a storm")
for _, t in ipairs({ dusk, night, storm }) do
  for i = 1, 3 do expect(t[i] >= 1 - mood.MAX_TINT - 1e-9, "within 20 %") end
end
local fire = kit.mana("fire")
local colours = {}
local real = render.ui_nine
render.ui_nine = function(tex, x, y, w, h, opts)
  colours[#colours + 1] = opts.color
  return real(tex, x, y, w, h, opts)
end
kit.set_tint(night)
kit.frame("thread", 0, 0, 20, 20)
kit.configure({ contrast = true })
kit.frame("thread", 0, 0, 20, 20)
kit.configure({})
kit.set_tint(nil)
render.ui_nine = real
expect(math.abs(colours[1][3] - night[3]) < 1e-6 and colours[1][1] < 1, "the frame takes the night's tint")
expect(colours[2][1] == 1 and colours[2][3] == 1, "high contrast is not tinted")
local fire_after = kit.mana("fire")
expect(fire[1] == fire_after[1] and fire[2] == fire_after[2], "the mana colours are untouched")

-- A wolf near: the edges glow; a hit from the left marks it; low health beats.
local c = game.carpet
local wolf = sim.spawn_creature("wolf", world.move(c.pos, 0, 10, 0, -(c.pos.alt - world.ground_height(c.pos))))
step(10)
expect(game.mood.danger > 0 and mood.edge(game.mood) > 0, "the edges glow with a wolf near")
expect_eq(mood.side(0, 270), "left")
expect_eq(mood.side(0, 90), "right")
expect_eq(mood.side(0, 180), "back")
mood.hit(game.mood, "left", engine.time())
step(2)
expect(#game.mood.hits == 1 and game.mood.hits[1].side == "left", "the left edge is marked")
step(60)
expect_eq(#game.mood.hits, 0, "for a moment")
sim.remove_creature(wolf)
game.health.hp = game.health.max * 0.2
step(3)
local a = mood.edge(game.mood)
step(20)
local b = mood.edge(game.mood)
expect(game.mood.low and a > 0 and b > 0 and math.abs(a - b) > 1e-3, "low health beats")
game.health.hp = game.health.max

-- The UI's sounds.
local played = #game.sound.played
local function heard(name)
  for i = played + 1, #game.sound.played do if game.sound.played[i] == name then return true end end
  return false
end
demo.pack.say(game.pack, "Taken: a feather")
step(3)
expect(heard("ui_cloth"), "a toast's cloth")
input.inject("quit", 1, 1)
step(3)
input.inject("tab_next", 1, 1)
step(3)
expect(heard("ui_page"), "a page turning")
input.inject("quit", 1, 1)
step(3)
input.inject("quit", 1, 1)
step(3)
local P = game.world_places.places
for _ = 1, 900 do
  step(1)
  if P.ready then break end
  sleep(5)
end
input.inject("story", 1, 1)
step(5)
expect(heard("ui_goal"), "a new goal's chime")
