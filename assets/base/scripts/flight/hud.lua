-- SPDX-License-Identifier: MIT
-- Heads-up text of the flight game.
local M = {}

local COMPASS = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" }

function M.compass(heading)
  local deg = math.deg(heading) % 360
  return COMPASS[math.floor((deg + 22.5) / 45) % 8 + 1]
end

function M.lines(c, ground, mode, info)
  local deg = math.floor(math.deg(c.heading) % 360 + 0.5) % 360
  local lines = {
    string.format("Zauberer %s - first flight", info.version),
    string.format("speed %3d km/h%s  heading %03d %s", math.floor(c.speed * 3.6 + 0.5), c.boosting and " BOOST" or "",
      deg, M.compass(c.heading)),
    string.format("altitude %d m  above ground %d m", math.floor(c.pos.alt + 0.5),
      math.floor(c.pos.alt - math.max(ground, 0) + 0.5)),
    string.format("flown %.1f km", c.distance / 1000),
  }
  if mode == "touch" then
    lines[#lines + 1] = "Left stick: steer | Right stick: climb | BOOST"
  else
    lines[#lines + 1] = "W/S: speed  A/D: turn  R/F: climb  Shift: boost  Mouse: look  F1: credits"
  end
  return lines
end

return M
