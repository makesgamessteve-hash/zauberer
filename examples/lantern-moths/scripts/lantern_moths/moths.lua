-- SPDX-License-Identifier: MIT
-- Lantern moths: at night a few gather round the heir; every so often one
-- leaves its dust on a sleeve. Plain Lua with no engine calls, so its tests
-- need no world.
local moths = {}

moths.DUST = { id = "moth_dust", name = "Moth-wing dust", kind = "material", weight = 0.01, price = 2 }
moths.EVERY = 20  -- seconds of night between two gifts of dust
moths.FLOCK = 5   -- moths round the heir at night

function moths.new() return { t = 0, given = 0 } end

-- One tick: returns how much dust the moths leave now (0 or 1).
function moths.tick(m, dt, player)
  if not player.night then
    m.t = 0
    return 0
  end
  m.t = m.t + dt
  if m.t < moths.EVERY then return 0 end
  m.t = m.t - moths.EVERY
  m.given = m.given + 1
  return 1
end

function moths.hud_lines(_, player)
  if not player.night then return {} end
  return { string.format("Lantern moths: %d around you", moths.FLOCK) }
end

-- Where to draw the moths (offsets in metres round the heir) at a time.
function moths.places(time)
  local out = {}
  for i = 1, moths.FLOCK do
    local a = time * (0.7 + 0.1 * i) + i * 1.3
    out[i] = { math.cos(a) * (1.5 + 0.2 * i), 1.2 + 0.3 * math.sin(a * 2 + i), math.sin(a) * (1.5 + 0.2 * i) }
  end
  return out
end

return moths
