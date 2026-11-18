-- SPDX-License-Identifier: MIT
-- Heads-up text of the flight game.
local M = {}

local COMPASS = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" }

function M.compass(heading)
  local deg = math.deg(heading) % 360
  return COMPASS[math.floor((deg + 22.5) / 45) % 8 + 1]
end

-- "day 3 of spring, 14:05" from world.time()
function M.date(t)
  return string.format("day %d of %s, %02d:%02d", t.day, t.season, t.hour, t.minute)
end

function M.lines(c, ground, mode, info, time)
  local deg = math.floor(math.deg(c.heading) % 360 + 0.5) % 360
  local lines = {
    string.format("Zauberer %s - first flight", info.version),
    string.format("speed %3d km/h%s  heading %03d %s", math.floor(c.speed * 3.6 + 0.5), c.boosting and " BOOST" or "",
      deg, M.compass(c.heading)),
    string.format("altitude %d m  above ground %d m", math.floor(c.pos.alt + 0.5),
      math.floor(c.pos.alt - math.max(ground, 0) + 0.5)),
    string.format("flown %.1f km", c.distance / 1000) .. (time and ("  " .. M.date(time)) or ""),
  }
  if mode == "touch" then
    lines[#lines + 1] = "Left stick: steer | Right stick: climb | BOOST"
  else
    lines[#lines + 1] = "W/S: speed  A/D: turn  R/F: climb  Shift: boost  Mouse: look  F1: credits"
  end
  return lines
end

return M
