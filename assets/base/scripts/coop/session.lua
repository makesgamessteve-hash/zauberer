-- SPDX-License-Identifier: MIT
-- Sessions (concept 11.6): a host shares a world through the relay (an
-- invite code) or on the LAN; others join by code, from the LAN list or by
-- address. On a first join the host sends the world: its bundle (a created
-- world) and, for a Rise and fall world, its history; the joiner keeps
-- them in its cache by hash and needs them no more. Packages must match:
-- a refused join lists the differences. Heirs drop in and out; when the
-- host leaves, the session ends and the world is saved.
local session = {}

session.RELAY = "ws://127.0.0.1:47821"

function session.new()
  return { phase = "alone", code = nil, error = nil, world = nil }
end

-- Hosts through the relay (net.host_relay) or the LAN (net.host and an
-- announcement). Returns true, or nil and why.
function session.host(S, netapi, how, opts)
  opts = opts or {}
  local ok, err
  if how == "relay" then
    ok, err = netapi.host_relay(opts.url or session.RELAY)
  else
    ok, err = netapi.host(opts.port or 0)
    if ok and netapi.lan_announce then netapi.lan_announce(true, opts.name or "Zauberer") end
  end
  if not ok then S.error = err return nil, err end
  S.phase, S.how = "hosting", how
  return true
end

-- Joins by invite code through the relay, or by address.
function session.join(S, netapi, how, what, url)
  local ok, err
  if how == "code" then ok, err = netapi.join_code(url or session.RELAY, what)
  else ok, err = netapi.join(what) end
  if not ok then S.error = err return nil, err end
  S.phase = "joining"
  return true
end

-- What the host sends a joiner first: the world's name, seed, and (as
-- blobs) the bundle and the history when there are any.
function session.welcome(netapi, player, world_info)
  netapi.send("world", { seed = world_info.seed, name = world_info.name, history = world_info.history ~= nil,
    bundle = world_info.bundle ~= nil, hash = world_info.bundle and netapi.blob_hash(world_info.bundle) }, player)
  if world_info.bundle then netapi.send_blob(player, "bundle", world_info.bundle) end
  if world_info.history then netapi.send_blob(player, "history", world_info.history) end
end

-- One tick: follows the session's messages. Returns what happened:
-- "joined", "welcome", "world", "ended" (with why and mismatches), "left".
function session.tick(S, netapi, msgs)
  local events = {}
  local st = netapi.status()
  if S.phase == "joining" and st.role == "client" then S.phase = "playing" events[#events + 1] = { what = "joined" } end
  if S.phase == "hosting" and st.invite_code then S.code = st.invite_code end
  for _, m in ipairs(msgs) do
    if m.event == "ended" then
      S.phase, S.error, S.mismatches = "alone", m.error, m.mismatches
      events[#events + 1] = { what = "ended", why = m.error, mismatches = m.mismatches }
    elseif m.event == "joined" and S.phase == "hosting" then
      events[#events + 1] = { what = "welcome", player = m.player }
    elseif m.event == "left" then
      events[#events + 1] = { what = "left", player = m.player }
    elseif m.event == "data" and m.channel == "world" then
      S.world = m.value
      S.waiting = {}
      if m.value.bundle then
        if netapi.blob_cached(m.value.hash) then
          S.bundle = netapi.blob_load(m.value.hash)
        else
          S.waiting.bundle = true
        end
      end
      if m.value.history then S.waiting.history = true end
      events[#events + 1] = { what = "world" }
    elseif m.event == "blob" then
      if m.name == "bundle" then S.bundle = m.data; netapi.blob_store(m.data) end
      if m.name == "history" then S.history = m.data end
      if S.waiting then S.waiting[m.name] = nil end
    elseif m.event == "blob_progress" then
      S.progress = { name = m.name, done = m.received, total = m.total }
    end
  end
  if S.world and S.waiting and next(S.waiting) == nil then
    S.waiting = nil
    events[#events + 1] = { what = "ready" }
  end
  return events
end

-- A line for the HUD.
function session.line(S)
  if S.phase == "hosting" then return S.code and ("Invite code: " .. S.code) or "Hosting" end
  if S.progress and S.waiting then
    return string.format("Receiving the world: %d%%", math.floor(100 * S.progress.done / math.max(1, S.progress.total)))
  end
  if S.error then return "Co-op: " .. S.error end
  return nil
end

return session
