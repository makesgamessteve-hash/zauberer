-- SPDX-License-Identifier: MIT
-- Lantern moths, the example mod. It uses the game's extension API (the
-- global `zauberer`, docs/modding/game-api.md) and the engine's (render,
-- world, engine.on): an item, a line on the HUD, work every tick, a message,
-- and something drawn every frame. Its German text is in locale/de/.
local moths = require("lantern_moths.moths")

local state = moths.new()
local butterfly -- the base game's sprite (packages can use each other's files)
local near_heir = false

assert(zauberer.add_item(moths.DUST))

zauberer.on_tick(function(dt, player)
  near_heir = player.night and player.pos ~= nil
  if moths.tick(state, dt, player) > 0 and zauberer.give("moth_dust", 1) > 0 then
    zauberer.say("A moth leaves its dust on your sleeve")
  end
end)

zauberer.add_hud(function(player) return moths.hud_lines(state, player) end)

engine.on("init", function()
  butterfly = render.sprite("sprites/butterfly.lua")
end)

engine.on("frame", function()
  local p = zauberer.player()
  if not (near_heir and butterfly and p.pos) then return end
  local at = world.to_local(p.pos)
  for _, o in ipairs(moths.places(engine.time())) do
    render.draw_sprite(butterfly, { at[1] + o[1], at[2] + o[2], at[3] + o[3] }, 0.5)
  end
end)

engine.on("hash", function() return state.given end) -- (part of the replayed state)
