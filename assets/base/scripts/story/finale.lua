-- SPDX-License-Identifier: MIT
-- Act III, "The Reweaving" (concept 1.6): Azhdar's citadel drifts on a ley
-- storm round the strongest nexus; divination (with three Weaves held, or
-- deep ritual mastery) reveals where it is now. To reach it the heirs need
-- a ritual to calm the storm (ritual mastery), an artifact against his ash
-- (an ash ward) and a summoned ally. The duel: Azhdar, warded against fire
-- and decay, weak to water and light, strikes with ash; a calmed storm
-- weakens his ward. Then the heirs choose an ending; the Reweave needs
-- deep Ritual and Void mastery.
local duel = require("story.duel")
local narrative = require("story.narrative")

local finale = {}

finale.DRIFT = 20000       -- metres from the nexus the citadel circles at
finale.PERIOD = 6 * 86400  -- seconds for one round
finale.RITUAL = 3          -- ritual mastery (tier) to calm the storm
finale.AZHDAR = { name = "Azhdar, the Ash Weaver", hp = 600, weak = { "water", "light" }, strong = { "fire", "decay" },
  attack = 6, every = 2.5, colour = "fire" }

-- Where the citadel is at a time: circling the strongest nexus.
function finale.citadel_at(nexus, time)
  local a = (time / finale.PERIOD) % 1 * 2 * math.pi
  local p = world.move(nexus, a, finale.DRIFT, 0, 0)
  p.alt = math.max(world.height(p), 0) + 600
  return p
end

-- Divination: where the citadel is, if the heirs can see it (three Weaves,
-- or ritual mastery deep enough without them).
function finale.divine(N, nexus, time, ritual_tier)
  if narrative.held(N) >= 3 or (ritual_tier or 0) >= finale.RITUAL + 1 then
    N.divined = true
    return finale.citadel_at(nexus, time)
  end
  return nil, "the storm hides it: hold three Weaves, or master ritual deeper"
end

-- Whether the heirs are ready for the storm. h: { ritual (tier), ash_ward
-- (an artifact working), ally (a summoned being) }. Returns true, or nil
-- and what is missing.
function finale.ready(h)
  local missing = {}
  if (h.ritual or 0) < finale.RITUAL then missing[#missing + 1] = "ritual mastery to calm the storm" end
  if not h.ash_ward then missing[#missing + 1] = "an ash ward" end
  if not h.ally then missing[#missing + 1] = "a summoned ally" end
  if #missing > 0 then return nil, table.concat(missing, ", ") end
  return true
end

function finale.duel()
  return duel.new(finale.AZHDAR)
end

-- The endings open to the heirs: Reweave needs ritual and void mastery.
-- h: { ritual (tier), void (has void weaves) }.
function finale.endings(h)
  local out = {}
  for _, id in ipairs({ "unravel", "bind", "reweave", "succession" }) do
    local e = narrative.data().endings[id]
    local ok = true
    if e.requires then ok = (h.ritual or 0) >= e.requires.ritual and (not e.requires.void or h.void) end
    if ok then out[#out + 1] = id end
  end
  return out
end

return finale
