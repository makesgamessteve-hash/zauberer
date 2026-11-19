-- SPDX-License-Identifier: MIT
-- The game's extension API for mods: the global `zauberer` (packages share
-- it through _G). A mod adds items, lines on the HUD and its own work every
-- tick, tells the player things and gives them items; a mod's function that
-- raises an error is turned off and reported, and the game goes on. See
-- docs/modding/game-api.md.
local items = require("life.items")

local api = {}

api.VERSION = "1.0" -- of this API (separate from the engine's scripting API)

-- game: { pack = function() -> the player's pack, player = function() -> facts, say = function(text) }
function api.new(game)
  local m = { game = game, huds = {}, ticks = {}, errors = {} }
  local z = { version = api.VERSION }

  -- Adds an item (the fields of data/items.lua); true, or nil and what is wrong.
  function z.add_item(def)
    return items.add(def)
  end

  -- fn(player) returns lines for the HUD (in English: they are translated with the packages' string tables).
  function z.add_hud(fn)
    assert(type(fn) == "function", "add_hud needs a function")
    m.huds[#m.huds + 1] = fn
  end

  -- fn(dt, player) runs every tick of play, after the game's own.
  function z.on_tick(fn)
    assert(type(fn) == "function", "on_tick needs a function")
    m.ticks[#m.ticks + 1] = fn
  end

  -- A message for the player.
  function z.say(text) game.say(tostring(text)) end

  -- Gives the player count of an item; returns how many fit in the pack.
  function z.give(id, count)
    if not items.get(id) then return 0 end
    return require("player.pack").add(game.pack(), id, math.max(0, math.floor(count or 1)))
  end

  -- What a mod may know of the player: { pos, on_foot, night, time }.
  function z.player() return game.player() end

  m.public = z
  return m
end

-- Calls fn safely: a failing function is turned off and reported once.
local function safely(m, list, i, ...)
  local ok, out = pcall(list[i], ...)
  if ok then return out end
  m.errors[#m.errors + 1] = tostring(out)
  engine.log("error", "a mod's function failed and is turned off: " .. tostring(out))
  list[i] = function() end
  return nil
end

function api.tick(m, dt)
  if #m.ticks == 0 then return end
  local p = m.game.player()
  for i = 1, #m.ticks do safely(m, m.ticks, i, dt, p) end
end

function api.hud_lines(m)
  local out = {}
  if #m.huds == 0 then return out end
  local p = m.game.player()
  for i = 1, #m.huds do
    local lines = safely(m, m.huds, i, p)
    for _, l in ipairs(type(lines) == "table" and lines or {}) do
      if type(l) == "string" then out[#out + 1] = l end
    end
  end
  return out
end

return api
