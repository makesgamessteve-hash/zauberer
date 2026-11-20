-- SPDX-License-Identifier: MIT
-- Starting the game over: the reset key (F9) pressed twice within a few
-- seconds starts the game again as at its first start (engine.reset). Saved
-- games are deleted; the player's settings, bindings and packages are kept.
local reset = {}

reset.WINDOW = 5 -- seconds to press the key again (and to show why it could not be done)

function reset.new()
  return { asked = nil, refused = nil, at = 0 }
end

-- The key's name as the player knows it ("F9"), from the action's bindings.
function reset.key_name(bindings)
  for _, b in ipairs(bindings or {}) do
    local key = b:match("^key:(.+)$")
    if key then return key:upper() end
  end
  return "?"
end

-- One tick: pressed (the reset key this tick), now (seconds), start_over(): true, or false and why.
-- Returns "ask" (the first press), "reset" (the second: the game starts over), "refused" or nil.
function reset.tick(r, pressed, now, start_over)
  if r.asked and now - r.asked > reset.WINDOW then r.asked = nil end
  if not pressed then return nil end
  if not r.asked then
    r.asked, r.refused = now, nil
    return "ask"
  end
  r.asked = nil
  local ok, why = start_over()
  if ok then return "reset" end
  r.refused, r.at = tostring(why), now
  return "refused"
end

-- The HUD's lines (translated where they are drawn).
function reset.lines(r, now, key)
  if r.asked and now - r.asked <= reset.WINDOW then
    return { string.format("Press %s again to start the game over (settings are kept)", key) }
  end
  if r.refused and now - r.at <= reset.WINDOW then
    return { string.format("The game cannot start over: %s", r.refused) }
  end
  return {}
end

return reset
