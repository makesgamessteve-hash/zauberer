-- SPDX-License-Identifier: MIT
-- Playing together (concept 11.4, 11.5): the world runs on the host's
-- clock. Sleep and time skips need every present heir in the pocket home,
-- or all of them agreeing in a vote; one outside can ask the others to
-- wait. Rituals channelled together go faster, each heir adding mana.
-- Creatures, predators and hostile mages grow with the heirs present; the
-- world's resources and mana do not. Spells harm whoever stands in them
-- (friendly fire is on by default; a world option softens it). A downed
-- heir can be revived by another with life magic or medicine before the
-- carpet carries them away to the home.
local together = {}

together.VOTE_TIME = 30     -- seconds a vote stays open
together.REVIVE_TIME = 60   -- seconds a downed heir can be revived
together.SOFT_FIRE = 0.25   -- harm to other heirs with friendly fire softened

-- Opens a vote on a time skip (kind: "sleep", "wait for", "ritual").
function together.vote(kind, by, present, now)
  local v = { kind = kind, by = by, yes = { [by] = true }, no = {}, present = present, opened = now }
  return v
end

function together.cast(v, heir, yes)
  if yes then v.yes[heir], v.no[heir] = true, nil else v.no[heir], v.yes[heir] = true, nil end
end

-- How a vote stands: "passed" (all present heirs are in the home or agreed),
-- "refused" (someone asked to wait), "expired" or "open".
function together.result(v, in_home, now)
  if next(v.no) ~= nil then return "refused" end
  local all = true
  for _, i in ipairs(v.present) do
    if not v.yes[i] and not (in_home and in_home[i]) then all = false end
  end
  if all then return "passed" end
  if now and now - v.opened > together.VOTE_TIME then return "expired" end
  return "open"
end

-- A ritual channelled by n heirs: how long it takes (each extra heir
-- shortens it) and the mana each of them gives.
function together.ritual(duration, mana, n)
  n = math.max(1, n)
  return duration / (1 + 0.5 * (n - 1)), mana / n
end

-- How the world answers n heirs: creature groups, predator boldness and
-- hostile mages grow; resources do not.
function together.scale(n)
  n = math.max(1, math.min(4, n))
  return { group = 1 + 0.35 * (n - 1), boldness = 1 + 0.2 * (n - 1), mage = 1 + 0.25 * (n - 1), resources = 1 }
end

-- The harm a spell does to an heir: full to whoever stands in it, or to
-- other heirs softened when the world says so.
function together.harm(amount, from, to, soft)
  if from ~= to and soft then return amount * together.SOFT_FIRE end
  return amount
end

-- A downed heir: revivable for REVIVE_TIME, then carried home by the carpet.
function together.down(heir, now)
  return { heir = heir, at = now }
end

-- Revives with "life magic" or "medicine" in time. Returns true, or nil and why.
function together.revive(d, by, means, now)
  if by == d.heir then return nil, "one cannot revive oneself" end
  if means ~= "life magic" and means ~= "medicine" then return nil, "it takes life magic or medicine" end
  if now - d.at > together.REVIVE_TIME then return nil, "too late: the carpet has carried them home" end
  d.revived = by
  return true
end

-- Whether the carpet has carried a downed heir home by now.
function together.carried(d, now)
  return not d.revived and now - d.at > together.REVIVE_TIME
end

return together
