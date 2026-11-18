-- SPDX-License-Identifier: MIT
-- The carpet as a cloak (concept 2.2): on command it wraps around the mage's
-- shoulders. It keeps them warm (it is worth up to 20 degrees, less when
-- wet), halves the wind's chill and keeps off rain. Its colour shifts on one
-- slider: drab blends with the land and the creatures see the mage less
-- (the visibility model: contrast and movement; the light the creatures
-- weigh themselves), vivid shows a mage, and awes. Falling while cloaked,
-- it spreads into a glider: stepping off the carpet high in the air is
-- safe, it glides down.
local cloak = {}

cloak.WARMTH = 20   -- deg C the cloak is worth, dry
cloak.ORDER = { "drab", "plain", "vivid" }
cloak.CONTRAST = { drab = 0.3, plain = 0.8, vivid = 1.4 }
cloak.PRESENCE = { drab = 1, plain = 1, vivid = 2.5 }
cloak.FLYING = 1.5  -- how visible the mage is on the carpet in the air

function cloak.new(saved)
  saved = type(saved) == "table" and saved or {}
  local colour = cloak.CONTRAST[saved.colour] and saved.colour or "plain"
  return { worn = saved.worn and true or false, colour = colour }
end

function cloak.snapshot(k)
  return { worn = k.worn or nil, colour = k.colour }
end

-- The next colour on the slider (drab, plain, vivid, drab ...).
function cloak.next_colour(k)
  for i, name in ipairs(cloak.ORDER) do
    if name == k.colour then
      k.colour = cloak.ORDER[i % #cloak.ORDER + 1]
      return k.colour
    end
  end
  k.colour = "plain"
  return k.colour
end

-- How the creatures perceive the mage: visibility (0 .. 2) and presence
-- (0 .. 3). ctx: { flying (on the carpet in the air), speed (m/s), silent
-- (the carpet's silent glide factor) }.
function cloak.seen(k, ctx)
  if ctx.flying then return math.min(cloak.FLYING * (ctx.silent or 1), 2), 1 end
  local speed = ctx.speed or 0
  local movement = speed < 0.3 and 0.6 or (speed > 3 and 1.3 or 1)
  local contrast = k.worn and cloak.CONTRAST[k.colour] or 1
  local presence = k.worn and cloak.PRESENCE[k.colour] or 1
  return math.max(math.min(contrast * movement, 2), 0), presence
end

-- How warm it keeps, by how wet the carpet is: 0 .. 1 of its worth.
function cloak.warmth(k, wetness)
  if not k.worn then return 0 end
  return 1 - 0.6 * (wetness or 0)
end

function cloak.hud_lines(k)
  if not k.worn then return {} end
  return { "Cloaked (" .. k.colour .. "): H to spread it out, . to shift its colour" }
end

return cloak
