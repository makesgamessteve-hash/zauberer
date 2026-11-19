-- SPDX-License-Identifier: MIT
-- Co-op flight for up to four heirs. The host is the authority: it simulates
-- every carpet, applying each client's inputs in order (one per tick, two
-- when behind), and sends snapshots. A client predicts its own carpet with
-- the same deterministic flight model and reconciles with the snapshots;
-- because the model is deterministic, a prediction for an acknowledged input
-- equals the host's state exactly, which is counted as a check. Both sides
-- also compare hashes of terrain they generate locally.
local carpet = require("flight.carpet")

local M = {}

M.STATE_EVERY = 3 -- ticks between snapshots (20 per second)
M.TERRAIN_EVERY = 120 -- ticks between terrain checks
M.MAX_CATCH_UP = 2 -- queued inputs a host applies per tick at most
M.SPACING = 15 -- metres between a joining heir and the host

function M.new()
  return {
    role = "none", me = -1, tick = 0, carpets = {}, queues = {}, acks = {}, inputs = {},
    seq = 0, history = {}, predicted_ok = 0, mispredictions = 0,
    terrain_checks = 0, terrain_mismatches = 0, inbox = {}, events = {}, riders = nil,
    rides = {}, rides_seen = 0,
  }
end

-- Copies a snapshot (carpet.snapshot) into a carpet.
function M.restore(c, s)
  c.pos = { face = s.face, x = s.x, y = s.y, lx = s.lx, ly = s.ly, alt = s.alt }
  c.heading, c.speed, c.pitch, c.vspeed, c.distance = s.heading, s.speed, s.pitch, s.vspeed, s.distance
end

local function same(a, b)
  for k, v in pairs(a) do
    if b[k] ~= v then return false end
  end
  return true
end
M.same = same

local function copy_input(inp, seq)
  return { seq = seq, turn = inp.turn or 0, throttle = inp.throttle or 0, climb = inp.climb or 0,
    boost = inp.boost or false, snap = inp.snap or 0, scale = inp.scale or 1 }
end

-- A carpet for a joining heir beside the host's.
local function spawn(co, player, api)
  local host = co.carpets[0]
  local pos = api.move(host.pos, host.heading, 0, M.SPACING * player, 0)
  co.carpets[player] = carpet.new(pos, host.heading)
  co.queues[player], co.acks[player], co.inputs[player] = {}, 0, copy_input({}, 0)
end

local function host_receive(co, m, api)
  if m.event ~= "data" then co.events[#co.events + 1] = m end
  if m.event == "joined" then
    spawn(co, m.player, api)
  elseif m.event == "left" then
    co.carpets[m.player], co.queues[m.player], co.acks[m.player], co.inputs[m.player] = nil, nil, nil, nil
  elseif m.event == "data" and m.channel == "input" and co.queues[m.player] and type(m.value) == "table" then
    local q = co.queues[m.player]
    q[#q + 1] = m.value
  elseif m.event == "data" then
    co.inbox[#co.inbox + 1] = m
  end
end

local function client_state(co, s, dt, api)
  for p, snap in pairs(s.carpets) do
    if p ~= co.me then
      co.carpets[p] = co.carpets[p] or carpet.new(snap, 0)
      M.restore(co.carpets[p], snap)
      co.inputs[p] = s.inputs[p] or co.inputs[p]
    end
  end
  local snap = s.carpets[co.me]
  if not snap then return end
  local mine = co.carpets[co.me]
  local ack = s.acks[co.me] or 0
  co.riders = s.riders
  if co.riders and co.riders.pilot ~= co.me and mine then -- (riding: carried by the pilot, nothing to predict)
    M.restore(mine, snap)
    for seq in pairs(co.history) do -- (inputs not yet applied stay: the Weave may be split before they are)
      if seq <= ack then co.history[seq] = nil end
    end
    co.seq = math.max(co.seq, ack)
    return
  end
  if not mine then
    mine = carpet.new(snap, 0)
    co.carpets[co.me] = mine
    M.restore(mine, snap)
    co.seq = ack
    return
  end
  local rides = s.rides and s.rides[co.me] or 0
  local predicted = co.history[ack]
  -- (A prediction is checked unless the host moved the carpet for riding since it was made.)
  if predicted and predicted.snap and predicted.rides == rides then
    if same(snap, predicted.snap) then
      co.predicted_ok = co.predicted_ok + 1
    else
      co.mispredictions = co.mispredictions + 1
    end
  end
  -- Reconcile: start from the host's state and replay the unacknowledged inputs.
  co.rides_seen = rides
  M.restore(mine, snap)
  for seq = ack + 1, co.seq do
    local h = co.history[seq]
    if h then
      carpet.update(mine, h.inp, dt, api)
      h.snap, h.rides = carpet.snapshot(mine), rides
    end
  end
  for seq in pairs(co.history) do
    if seq <= ack then co.history[seq] = nil end
  end
end

local function client_receive(co, m, dt, api)
  if m.event ~= "data" then co.events[#co.events + 1] = m end
  if m.event == "data" and m.channel == "state" then
    client_state(co, m.value, dt, api)
  elseif m.event == "data" and m.channel == "terrain" then
    co.terrain_checks = co.terrain_checks + 1
    if api.chunk_hash(m.value.pos) ~= m.value.hash then
      co.terrain_mismatches = co.terrain_mismatches + 1
    end
  elseif m.event == "left" then
    co.carpets[m.player], co.inputs[m.player] = nil, nil
  elseif m.event == "data" then
    co.inbox[#co.inbox + 1] = m
  end
end

-- Riding the whole Weave together: the riders sit on the pilot's carpet
-- (each a little behind the last), whatever their own inputs.
local function ride(co, api)
  local r = co.riders
  if not r then return end
  local pilot = co.carpets[r.pilot]
  if not pilot then return end
  for k, p in ipairs(r.list) do
    local c = co.carpets[p]
    if c and p ~= r.pilot then
      co.rides[p] = (co.rides[p] or 0) + 1 -- (the client cannot foresee this move)
      c.pos = api.move(pilot.pos, pilot.heading, -1.2 * k, 0, 0)
      c.pos.alt = pilot.pos.alt
      c.heading, c.speed, c.pitch, c.vspeed = pilot.heading, pilot.speed, pilot.pitch, pilot.vspeed
    end
  end
end

local function host_tick(co, own, inp, dt, api, netapi)
  co.carpets[0] = own
  carpet.update(own, inp, dt, api)
  co.inputs[0] = copy_input(inp, co.tick)
  for p, q in pairs(co.queues) do
    local n = #q > M.MAX_CATCH_UP and M.MAX_CATCH_UP or #q
    for _ = 1, n do
      local i = table.remove(q, 1)
      carpet.update(co.carpets[p], i, dt, api)
      co.acks[p], co.inputs[p] = i.seq, i
    end
  end
  ride(co, api)
  if co.tick % M.STATE_EVERY == 0 then
    local snaps = {}
    for p, c in pairs(co.carpets) do
      snaps[p] = carpet.snapshot(c)
    end
    netapi.send("state", { tick = co.tick, carpets = snaps, acks = co.acks, inputs = co.inputs, riders = co.riders,
      rides = co.rides })
  end
  if co.tick % M.TERRAIN_EVERY == 0 then
    local pos = { face = own.pos.face, x = own.pos.x, y = own.pos.y, lx = 128, ly = 128, alt = 0 }
    netapi.send("terrain", { pos = pos, hash = api.chunk_hash(pos) })
  end
end

local function client_tick(co, inp, dt, api, netapi)
  for p, c in pairs(co.carpets) do -- others: dead reckoning with their last input
    if p ~= co.me then
      carpet.update(c, co.inputs[p] or {}, dt, api)
    end
  end
  local mine = co.carpets[co.me]
  if not mine then return end
  co.seq = co.seq + 1
  local i = copy_input(inp, co.seq)
  if not (co.riders and co.riders.pilot ~= co.me) then
    carpet.update(mine, i, dt, api)
    co.history[co.seq] = { inp = i, snap = carpet.snapshot(mine), rides = co.rides_seen }
  else -- (riding: nothing predicted, but the input is kept in case the host has split the Weave already)
    co.history[co.seq] = { inp = i }
  end
  netapi.send("input", i, 0)
end

-- The heirs ride the whole Weave: pilot (a player index) and the riders
-- (player indices, the pilot among them); nil: each flies their own.
function M.set_riders(co, pilot, list)
  co.riders = list and { pilot = pilot, list = list } or nil
end

-- Takes the session events seen since last asked (joined, left, welcome,
-- ended, away, back, blobs).
function M.take_events(co)
  local e = co.events
  co.events = {}
  return e
end

-- One tick. own: the local carpet when hosting or alone; api: { move, height,
-- chunk_hash }; netapi: { status, receive, send }. Returns this heir's carpet.
function M.tick(co, own, inp, dt, api, netapi)
  co.tick = co.tick + 1
  local st = netapi.status()
  if st.role ~= co.role and st.role == "host" then
    co.carpets = { [0] = own }
  end
  co.role, co.me = st.role, st.player
  for _, m in ipairs(netapi.receive()) do
    if co.role == "host" then
      host_receive(co, m, api)
    else
      client_receive(co, m, dt, api)
    end
  end
  if co.role == "host" then
    host_tick(co, own, inp, dt, api, netapi)
    return own
  elseif co.role == "client" then
    client_tick(co, inp, dt, api, netapi)
    return co.carpets[co.me] or own
  end
  carpet.update(own, inp, dt, api)
  return own
end

return M
