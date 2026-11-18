-- SPDX-License-Identifier: MIT
-- Mana from Lua: read the field by the carpet, draw from it until it is thin,
-- give some back, and find the ley network.
-- spec: mana: Mana from Lua
scenario { name = "mana" }
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if mana.at(here).loaded then break end
  sleep(5)
end
local cell = mana.at(here)
expect(cell.loaded, "the carpet's ground has mana")
local colors = mana.colors()
expect_eq(#colors, 8)
expect_eq(colors[1], "fire")
expect_eq(colors[5], "water") -- opposite fire, four along the wheel
local total = 0
for _, c in ipairs(colors) do
  expect(cell.stored[c] >= 0 and cell.cap[c] >= 0, c)
  total = total + cell.cap[c]
end
expect(total > 0, "some mana here")
-- The richest colour by the carpet, drawn down.
local richest, most = nil, -1
for _, c in ipairs(colors) do
  if cell.stored[c] > most then
    richest, most = c, cell.stored[c]
  end
end
local got = mana.draw(here, richest, most / 2)
expect_near(got, most / 2, 1e-6 * most + 1e-9)
expect(mana.at(here).stored[richest] < most * 0.6, "thinner")
expect(mana.draw(here, richest, 1e6, 64) > 0, "more from around")
mana.release(here, richest, 10)
local st = mana.stats()
expect(st.chunks > 0 and st.drawn > most / 2, "the draw is counted")
expect(st.cap[richest] > 0)
expect(sim.tier_stats().mana_ms >= 0)
-- The ley network: somewhere on the planet a nexus with two colours.
local ley = mana.ley(here)
expect(ley.line_distance >= 0 and ley.nexus_distance >= 0 and ley.strength == 1)
expect_eq(#ley.nexus_colors, 2)
expect(ley.nexus_colors[1] ~= ley.nexus_colors[2])
expect_eq(#ley.line_colors, 2)
expect(mana.ley(ley.nexus).nexus_distance < 1, "the nexus is where it says")
-- Mistakes.
expect_error(function() mana.draw(here, "plasma", 1) end, "color must be")
expect_error(function() mana.draw(here, "fire", -1) end, "amount must be 0..1e6")
expect_error(function() mana.draw(here, "fire", 1, 1000) end, "radius must be 0..256 metres")
expect_error(function() mana.release(here, "fire", -5) end, "amount must be 0..1e6")
