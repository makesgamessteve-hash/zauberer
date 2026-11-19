-- SPDX-License-Identifier: MIT
-- One carpet, several heirs (concept 11.2). Together they ride the whole
-- Wandering Weave: one pilots, the others cast in every direction; it has
-- its full lift, speed and mana. It can unravel into one smaller carpet per
-- heir, each bonded to its heir, weaker by its share and the heir's bond;
-- splitting and rejoining take a moment, and rejoining needs the heirs
-- close together. Each piece is that heir's cloak. The tent is pitched only
-- from the whole Weave (unless the Homing upgrade brings a piece back on
-- its own). Upgrades woven into the Weave are in every piece; damage done
-- to a piece is the whole's when it rejoins.
local weave = {}

weave.SPLIT_TIME = 2    -- seconds to unravel or rejoin
weave.REJOIN = 20       -- metres within which the pieces can rejoin
weave.PIECE = { lift = 0.6, speed = 0.85, ceiling = 0.7 } -- a piece's share of the whole, at a full bond

function weave.new(saved)
  saved = type(saved) == "table" and saved or {}
  return { whole = saved.whole ~= false, pieces = saved.pieces or {}, pilot = saved.pilot or 1,
    damage = tonumber(saved.damage) or 0, upgrades = saved.upgrades or {}, busy = 0 }
end

function weave.snapshot(W)
  return { whole = W.whole, pieces = W.pieces, pilot = W.pilot, damage = W.damage, upgrades = W.upgrades }
end

-- Riding together: who pilots (the one asked, if present) and who casts.
function weave.ride(W, present, pilot)
  W.pilot = pilot or W.pilot
  local ok = false
  for _, i in ipairs(present) do ok = ok or i == W.pilot end
  if not ok then W.pilot = present[1] end
  local casters = {}
  for _, i in ipairs(present) do
    if i ~= W.pilot then casters[#casters + 1] = i end
  end
  return W.pilot, casters
end

-- What a carpet can do: the whole Weave's full figures, or a piece's share
-- scaled by its heir's bond (0..1). base: { lift, speed, ceiling, reservoir }.
function weave.stats(W, base, heir, bond)
  if W.whole then
    return { lift = base.lift, speed = base.speed, ceiling = base.ceiling, reservoir = base.reservoir }
  end
  local n = math.max(1, #W.order)
  local b = 0.5 + 0.5 * math.max(0, math.min(1, bond or 1))
  local _ = heir
  return { lift = base.lift * weave.PIECE.lift * b, speed = base.speed * weave.PIECE.speed * b,
    ceiling = base.ceiling * weave.PIECE.ceiling * b, reservoir = base.reservoir / n * b }
end

-- Unravels into one piece per present heir (takes SPLIT_TIME). Returns the
-- pieces, or nil and why.
function weave.split(W, present)
  if not W.whole then return nil, "the Weave is already unravelled" end
  if #present < 2 then return nil, "one heir rides the whole Weave" end
  if W.busy > 0 then return nil, "the Weave is still moving" end
  W.whole, W.busy, W.order, W.pieces = false, weave.SPLIT_TIME, {}, {}
  for _, i in ipairs(present) do
    W.order[#W.order + 1] = i
    W.pieces[i] = { heir = i, damage = 0, cloak = { colour = i } }
  end
  return W.pieces
end

-- Rejoins the pieces: only when every heir's position is within REJOIN of
-- the others'. Damage to the pieces is the whole's.
function weave.rejoin(W, positions, distance)
  if W.whole then return nil, "the Weave is whole" end
  for _, a in pairs(positions) do
    for _, b in pairs(positions) do
      if distance(a, b) > weave.REJOIN then return nil, "come closer together to rejoin" end
    end
  end
  for _, p in pairs(W.pieces) do W.damage = W.damage + p.damage end
  W.whole, W.busy, W.pieces, W.order = true, weave.SPLIT_TIME, {}, nil
  return true
end

-- Damage to a carpet: the whole's, or a piece's (until it rejoins).
function weave.hurt(W, heir, amount)
  if W.whole then W.damage = W.damage + amount
  elseif W.pieces[heir] then W.pieces[heir].damage = W.pieces[heir].damage + amount end
end

-- Whether the tent can be pitched: from the whole Weave, or with Homing.
function weave.can_pitch(W)
  if W.whole or W.upgrades.homing then return true end
  return false, "the Weave must be whole to pitch the tent"
end

-- An upgrade woven into the Weave is in every piece.
function weave.upgrade(W, id)
  W.upgrades[id] = true
end

function weave.has(W, id)
  return W.upgrades[id] == true
end

function weave.tick(W, dt)
  W.busy = math.max(0, W.busy - dt)
end

return weave
