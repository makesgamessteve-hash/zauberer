-- SPDX-License-Identifier: MIT
-- The spellbook in play: the hands hold Fireball and Kindle from the first
-- books; holding a hand button charges and letting go casts; words and a
-- drawn circle make a free weave that is discovered, journalled and put in the
-- right hand; the quick wheel swaps weaves; the HUD shows it all.
-- spec: spellbook: Casting from the hands
-- spec: spellbook: Free weaving and discovery
scenario { name = "spellbook" }
local game = demo.game
local magic = demo.magic
local spellbook = require("magic.spellbook")
for _ = 1, 900 do
  step(1)
  if sim.fire_at(game.carpet.pos).loaded then break end
  sleep(5)
end
local p = game.magic
local book = p.caster.book
expect_eq(#spellbook.list(book), 4, "four weaves from Farrokh's books")
expect_eq(spellbook.in_slot(book, "left").form, "line")
expect_eq(spellbook.in_slot(book, "right").form, "tap")
local function hud_has(text)
  for _, line in ipairs(magic.hud_lines(p)) do
    if line:find(text, 1, true) then return true end
  end
  return false
end
expect(hud_has("Left: Fireball") and hud_has("Right: Kindle"), "the hands on the HUD")
expect(hud_has("Mana "), "the mana on the HUD")
-- Hold the right hand for half a second: Kindle charges, then casts.
local fire_before = p.caster.mage.mana.fire
input.inject("cast_right", 1, 0, 30)
step(10)
expect(p.charging and p.charging.hand == "right", "charging")
expect(hud_has("Charging"), "the charge on the HUD")
step(25)
expect(not p.charging, "cast on release")
local r = game.last_cast
expect(r and r.ok and r.name == "Kindle", r and r.message)
expect(p.caster.mage.mana.fire < fire_before, "paid in Fire")
expect(spellbook.casts(book, r.plan.weave) == 1)
expect(hud_has("Kindle"), "the cast on the HUD")
sim.cool(game.carpet.pos, 20, 1e9)
-- Free weaving: the words "nehl atash" (unmake fire), a circle drawn with the mouse.
for _ = 1, 1 do
  input.inject("verb", 1, 0, 1)
  step(2)
end
expect_eq(magic.free_weave(p).verb, "unmake")
expect_eq(magic.free_weave(p).colors[1], "fire")
input.inject("color2", 1, 0, 1)
step(2)
expect_eq(#magic.free_weave(p).colors, 1, "the same colour twice is one colour")
input.inject("color2", 1, 0, 1)
step(2)
expect_eq(magic.free_weave(p).colors[2], "storm", "a second colour can be picked")
for _ = 1, 7 do -- round the wheel back to none
  input.inject("color2", 1, 0, 1)
  step(2)
end
expect_eq(#magic.free_weave(p).colors, 1)
local t = gesture.template("circle", 40)
input.inject("draw", 1, 0, 45)
step(1)
for i = 2, 40 do
  input.inject("look", (t[2 * i - 1] - t[2 * i - 3]) * 120, (t[2 * i] - t[2 * i - 2]) * 120, 1)
  step(1)
end
step(6)
expect_eq(p.free.form, "circle", "the drawn form")
expect(#magic.sights(p).trail == 0, "no trail once drawn")
expect(hud_has("Free weave: unmake fire circle"))
input.inject("cast_left", 1, 0, 40)
step(45)
r = game.last_cast
expect(r and r.ok and r.name == "Unmake Fire" and r.discovered, r and r.message)
expect(hud_has("You discovered Unmake Fire"))
expect_eq(spellbook.in_slot(book, "right").verb, "unmake", "a new weave goes to the right hand")
expect(not p.free.form, "the free weave is spent")
-- A scribble that is no form, and a gesture drawn while charging.
local seed = 99
local function rand()
  seed = (seed * 1664525 + 1013904223) % 4294967296
  return (seed >> 8) % 200
end
local x, y = 0, 0
input.inject("draw", 1, 0, 122)
step(1)
for _ = 1, 120 do
  local nx, ny = rand(), rand()
  input.inject("look", nx - x, ny - y, 1)
  x, y = nx, ny
  step(1)
end
step(4)
expect(hud_has("That was no form"))
p.caster.mage.mana.fire = 30
input.inject("cast_right", 1, 0, 50)
step(5)
input.inject("draw", 1, 0, 30)
step(1)
for i = 2, 25 do
  input.inject("look", (t[2 * i - 1] - t[2 * i - 3]) * 120, (t[2 * i] - t[2 * i - 2]) * 120, 1)
  step(1)
end
step(30)
expect(game.last_cast.ok and game.last_cast.name == "Unmake Fire", game.last_cast.message)
-- The quick wheel: key 1 puts Kindle back in the right hand.
input.inject("wheel1", 1, 0, 1)
step(2)
expect_eq(spellbook.in_slot(book, "right").form, "tap")
expect(hud_has("Right hand: Kindle"))
-- With no mana the cast is refused, and the HUD says so.
for c in pairs(p.caster.mage.mana) do p.caster.mage.mana[c] = 0 end
input.inject("cast_right", 1, 0, 20)
step(25)
expect(not game.last_cast.ok and game.last_cast.message == "not enough mana")
expect(hud_has("not enough mana"))
-- Strain shows when it is high.
p.caster.mage.strain = 50
expect(hud_has("Strain 50"))
-- Saved with the game and restored.
local snap = magic.snapshot(p)
local again = magic.new(snap)
expect_eq(#spellbook.list(again.caster.book), 5)
expect_eq(spellbook.casts(again.caster.book, { verb = "unmake", colors = { "fire" }, form = "circle" }), 2)
expect_eq(again.caster.mage.strain, 50)
