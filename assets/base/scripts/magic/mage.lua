-- SPDX-License-Identifier: MIT
-- The mage's own mana (concept 3.2): one reservoir for all colours (mana keeps
-- its colour while stored), a channeling rate that limits how fast mana can be
-- drawn in or poured out, an attunement per colour (0 .. 100) that grows with
-- use and with time in places rich in that colour, and strain from pouring
-- out faster than the channeling rate allows.
local mage = {}

mage.COLORS = { "fire", "storm", "light", "life", "water", "earth", "void", "decay" }
local STRAIN_RECOVERY = 0.05 -- strain lost per second
local ATTUNE_PER_CAST = 0.5
local ATTUNE_PER_SECOND = 0.002 -- at a place holding 100 mana of the colour

function mage.new(options)
  options = options or {}
  local m = {
    capacity = options.capacity or 100,
    channel = options.channel or 10, -- mana per second
    mana = {},
    attunement = {},
    strain = 0,
  }
  for _, c in ipairs(mage.COLORS) do
    m.mana[c] = options.mana and options.mana[c] or 0
    m.attunement[c] = options.attunement and options.attunement[c] or 0
  end
  return m
end

function mage.total(m)
  local sum = 0
  for _, c in ipairs(mage.COLORS) do sum = sum + m.mana[c] end
  return sum
end

-- Stores up to amount mana of a colour; returns how much fitted.
function mage.add(m, color, amount)
  local room = math.max(m.capacity - mage.total(m), 0)
  local add = math.min(math.max(amount, 0), room)
  m.mana[color] = m.mana[color] + add
  return add
end

-- Whether the reservoir holds the mana of each colour in per.
function mage.has(m, per)
  for c, amount in pairs(per) do
    if (m.mana[c] or 0) + 1e-9 < amount then return false end
  end
  return true
end

-- Takes the mana of each colour in per; false (and nothing taken) when short.
function mage.take(m, per)
  if not mage.has(m, per) then return false end
  for c, amount in pairs(per) do
    m.mana[c] = math.max(m.mana[c] - amount, 0)
  end
  return true
end

-- Efficiency 0 .. 0.6 for a weave's colours: attunement and familiarity.
function mage.efficiency(m, colors, tier_bonus)
  local sum = 0
  for _, c in ipairs(colors) do sum = sum + (m.attunement[c] or 0) end
  return math.min(0.6, sum / #colors / 250 + (tier_bonus or 0))
end

-- How far a weave of power poured over seconds is beyond the channeling rate
-- (0 when within it); pouring beyond it builds strain.
function mage.overcharge(m, power, seconds)
  local limit = m.channel * math.max(seconds or 0, 0.25)
  return math.max(power / limit - 1, 0)
end

function mage.strain_by(m, overcharge)
  m.strain = math.min(m.strain + overcharge * 20, 100)
end

-- Strain over 60 blurs control; over 90 the mage burns out for a moment.
function mage.burnt_out(m)
  return m.strain >= 90
end

-- A cast of these colours attunes the mage a little more to them.
function mage.cast(m, colors)
  for _, c in ipairs(colors) do
    m.attunement[c] = math.min(m.attunement[c] + ATTUNE_PER_CAST, 100)
  end
end

-- Time passes at a place holding place[colour] mana: strain eases, and the
-- mage attunes slowly to the place's colours.
function mage.tick(m, dt, place)
  m.strain = math.max(m.strain - STRAIN_RECOVERY * dt, 0)
  for _, c in ipairs(mage.COLORS) do
    local here = place and place[c] or 0
    m.attunement[c] = math.min(m.attunement[c] + ATTUNE_PER_SECOND * dt * here / 100, 100)
  end
end

-- A copy with only plain values (for saves).
function mage.snapshot(m)
  local out = { capacity = m.capacity, channel = m.channel, strain = m.strain, mana = {}, attunement = {} }
  for _, c in ipairs(mage.COLORS) do
    out.mana[c] = m.mana[c]
    out.attunement[c] = m.attunement[c]
  end
  return out
end

return mage
