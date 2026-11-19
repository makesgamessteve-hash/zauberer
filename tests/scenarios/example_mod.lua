-- SPDX-License-Identifier: MIT
-- Runs with the example mod (examples/lantern-moths) installed: it loads
-- after the base game, adds its item, gives dust at night, shows its line on
-- the HUD in English and German; a mod function that fails is turned off and
-- the game goes on.
-- spec: modding-sdk: The example mod works
-- spec: modding-sdk: A failing mod function is turned off
scenario { name = "example mod" }
local game = demo.game
local i18n = require("ui.i18n")
local items = require("life.items")
local pack = require("player.pack")

local found
for _, p in ipairs(engine.packages()) do
  if p.id == "lantern-moths" then found = p end
end
expect(found and found.enabled and found.load_order > 1, "the mod is loaded after the base game")
expect(items.get("moth_dust") and items.get("moth_dust").name == "Moth-wing dust", "its item")
local again, why = zauberer.add_item({ id = "moth_dust", name = "x", kind = "material", weight = 1 })
expect(not again and why:find("twice"), "an item id is taken once")
expect(not zauberer.add_item({ id = "bad", name = "Bad", kind = "nonsense", weight = 1 }), "items are checked")

-- Night falls (for the mods' view of the player).
local player = game.mods.game.player
game.mods.game.player = function()
  local p = player()
  p.night = true
  return p
end
step(60 * 21)
expect(pack.count(game.pack, "moth_dust") >= 1, "dust in the pack")
local function hud_has(text)
  for _, line in ipairs(demo.flight_lines()) do
    if type(line) == "string" and line:find(text, 1, true) then return true end
  end
  return false
end
expect(hud_has("Lantern moths: 5 around you"), "its line on the HUD")
expect(hud_has("A moth leaves its dust on your sleeve"), "its message")
i18n.set("de")
expect_eq(i18n.t("Lantern moths: 5 around you"), "Laternenmotten: 5 um dich", "its German text")
expect_eq(i18n.t("Pack 0.01/20 kg: moth-wing dust 1"), "Gepäck 0.01/20 kg: Mottenflügelstaub 1")
i18n.set("en")

-- A failing mod function is turned off once; the game goes on.
local calls = 0
zauberer.on_tick(function()
  calls = calls + 1
  error("a mod's mistake")
end)
step(10)
expect_eq(calls, 1)
expect_eq(#game.mods.errors, 1)
expect(game.mods.errors[1]:find("a mod's mistake", 1, true))
game.mods.game.player = player
