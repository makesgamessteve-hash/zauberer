-- SPDX-License-Identifier: MIT
-- Co-op over real networks (implementation plan 1.11): other heirs drawn a
-- little in the past between snapshots so latency and jitter do not show;
-- the connection lost and rejoined within the host's grace with growing
-- waits; carpets and entities sent only to the heirs near them, the
-- nearest first, within the budget (the state channel keeps only the
-- newest); the replicated state hashed now and then on both sides, a
-- mismatch answered by the host sending that part whole, and logged.
local hardening = {}

hardening.DELAY = 0.1        -- seconds others are drawn in the past
hardening.GRACE = 30         -- seconds a lost connection can be rejoined in
hardening.INTEREST = 3000    -- metres within which entities are sent to an heir
hardening.HASH_EVERY = 300   -- ticks between state hashes

-- A buffer of an entity's snapshots by time.
function hardening.buffer()
  return { list = {} }
end

function hardening.push(buf, t, state)
  buf.list[#buf.list + 1] = { t = t, s = state }
  while #buf.list > 32 do table.remove(buf.list, 1) end
end

-- The state at time t (in the past): between the two snapshots around it,
-- interpolated by lerp(a, b, f); the last one if t is beyond.
function hardening.sample(buf, t, lerp)
  local list = buf.list
  if #list == 0 then return nil end
  if t <= list[1].t then return list[1].s end
  for k = 2, #list do
    if list[k].t >= t then
      local a, b = list[k - 1], list[k]
      return lerp(a.s, b.s, (t - a.t) / math.max(b.t - a.t, 1e-9))
    end
  end
  return list[#list].s
end

-- Rejoining after the connection is lost: tries at 1, 2, 4, 8 ... seconds
-- after the loss, until the grace runs out. api: { status(), rejoin() }.
function hardening.reconnect(R, now, api)
  local st = api.status()
  if st.role == "client" then R.lost, R.tries = nil, 0 return "connected" end
  if not st.can_rejoin and not R.lost then return "idle" end
  R.lost = R.lost or now
  R.tries = R.tries or 0
  if now - R.lost > hardening.GRACE then R.lost = nil return "gave up" end
  local wait = 2 ^ R.tries
  if now - R.lost >= wait - 1 and (R.last or -1) < now - 0.5 then
    R.last = now
    R.tries = R.tries + 1
    api.rejoin()
    return "trying"
  end
  return "waiting"
end

-- The entities worth sending to an heir: within INTEREST, nearest first.
function hardening.interest(pos, entities, distance)
  local out = {}
  for id, e in pairs(entities) do
    local d = distance(pos, e.pos)
    if d <= hardening.INTEREST then out[#out + 1] = { id = id, d = d } end
  end
  table.sort(out, function(a, b) return a.d < b.d or (a.d == b.d and a.id < b.id) end)
  return out
end

-- Hashes of the replicated state, part by part (parts: name -> value).
function hardening.hashes(parts, hash)
  local out = {}
  for k, v in pairs(parts) do out[k] = hash(v) end
  return out
end

-- The parts whose hashes differ.
function hardening.desync(mine, theirs)
  local out = {}
  for k, v in pairs(theirs) do
    if mine[k] ~= v then out[#out + 1] = k end
  end
  table.sort(out)
  return out
end

-- Sets the channels up for the budget: snapshots keep only the newest,
-- deltas and inputs are never dropped.
function hardening.channels(netapi)
  netapi.set_channel("state", "latest")
  netapi.set_channel("deltas", "reliable")
  netapi.set_channel("input", "reliable")
end

return hardening
