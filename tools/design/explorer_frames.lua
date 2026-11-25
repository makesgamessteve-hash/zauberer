-- SPDX-License-Identifier: MIT
-- Stills of the world explorer (ui/explorer.lua) over a created world: the
-- whole globe by biome, by realm, near with a realm's page open. Written into
-- ZB_GOLDEN_OUT as explorer_<name>.png. Run in a window (1280x720).
scenario { name = "explorer frames" }
local game = demo.game
local explorer = require("ui.explorer")
game.world_options = { radius = 400000, quality = "quick", seed = 11 }
game.set_mode("create")
for _ = 1, 20000 do
  step(1)
  if game.creation.progress and game.creation.progress.done then break end
  sleep(2)
end
input.inject("confirm", 1, 1)
step(2)
input.inject("confirm", 0, 1)
for _ = 1, 300 do
  step(1)
  if game.mode == "flight" then break end
end
step(10)
input.inject("explorer", 1, 1)
step(60)
local x = game.explorer
local function shot(name)
  step(40)
  render.match_golden("explorer_" .. name, "window")
  print("explorer frame " .. name)
end
shot("globe")
x.layer = 2
shot("realms")
explorer.push(x, { view = "list", of = "polity" })
explorer.push(x, { view = "entity", id = "polity:1" })
explorer.zoom(x, 0.35)
shot("realm")
x.layer = 1
explorer.zoom(x, 0.4)
shot("near")
