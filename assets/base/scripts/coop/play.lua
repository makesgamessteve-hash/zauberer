-- SPDX-License-Identifier: MIT
-- Co-op in the game: the heirs, the Weave, the session, the host's clock,
-- votes, the active areas and reconnecting, on top of flight.coop (which
-- moves the carpets). The host decides: who is present, whether the Weave
-- is whole and who pilots, when time skips; clients ask by message
-- ("ask": split, rejoin, vote, ballot) and follow the host's clock.
local heirs = require("coop.heirs")
local weave = require("coop.weave")
local session = require("coop.session")
local together = require("coop.together")
local areas = require("coop.areas")
local hardening = require("coop.hardening")
local coop = require("flight.coop")

local play = {}

play.CLOCK_EVERY = 60  -- ticks between the host's clock messages
play.AREAS_EVERY = 30  -- ticks between active-area updates
play.DRIFT = 1         -- seconds a client's clock may drift

function play.new(saved, count)
  saved = type(saved) == "table" and saved or {}
  return { heirs = heirs.new(count or saved.count or 4, saved.heirs), weave = weave.new(saved.weave),
    session = session.new(), reconnect = {}, ticks = 0, said = {} }
end

function play.snapshot(c)
  return { heirs = heirs.snapshot(c.heirs), weave = weave.snapshot(c.weave), count = c.heirs.count }
end

local function say(c, text) c.said[#c.said + 1] = text end

-- The heir index of a player (heirs are 1-based, players 0-based).
local function heir_of(player) return player + 1 end

local function present_players(c)
  local out = {}
  for _, i in ipairs(heirs.present(c.heirs)) do out[#out + 1] = i - 1 end
  return out
end

-- Keeps the riders of the whole Weave in flight.coop.
local function update_riding(c, co)
  local players = present_players(c)
  if c.weave.whole and #players > 1 then
    local pilot = weave.ride(c.weave, heirs.present(c.heirs)) - 1
    coop.set_riders(co, pilot, players)
  else
    coop.set_riders(co, nil, nil)
  end
end

-- Host: what a client asked for.
local function host_asked(c, co, m, ctx)
  local v = type(m.value) == "table" and m.value or {}
  if v.what == "split" then
    local ok, why = weave.split(c.weave, heirs.present(c.heirs))
    say(c, ok and "The Weave unravels into one carpet each" or why)
  elseif v.what == "rejoin" then
    local positions = {}
    for _, p in ipairs(present_players(c)) do positions[p] = co.carpets[p] and co.carpets[p].pos end
    local ok, why = weave.rejoin(c.weave, positions, world.distance)
    say(c, ok and "The Weave is whole again" or why)
  elseif v.what == "vote" then
    c.vote = together.vote(v.kind or "sleep", heir_of(m.player), heirs.present(c.heirs), ctx.now)
    if ctx.netapi then ctx.netapi.send("vote", { kind = c.vote.kind }) end
    say(c, "A vote to " .. c.vote.kind .. ": press sleep to agree")
  elseif v.what == "heir" then -- a player's own heir, as they made it
    local e = heirs.create(c.heirs, heir_of(m.player), v)
    if e then
      e.status = "present"
      say(c, e.name .. " is here")
    end
  elseif v.what == "ballot" and c.vote then
    together.cast(c.vote, heir_of(m.player), v.yes ~= false)
  end
end

-- One tick, after flight.coop's. ctx: { now (world seconds), netapi,
-- in_home (heir set), sleep(hours), world_info (seed, name, bundle,
-- history), tier (sim.tier_stats-like) }.
function play.tick(c, co, ctx)
  c.ticks = c.ticks + 1
  local netapi = ctx.netapi
  local role = co.role
  for _, ev in ipairs(coop.take_events(co)) do
    if role == "host" and ev.event == "joined" then
      local e, reunion = heirs.join(c.heirs, heir_of(ev.player), ctx.past_prologue)
      if not e then -- (a player whose heir is not made yet gets the next free one made for them)
        heirs.create(c.heirs, heir_of(ev.player), { sex = "girl", name = "Heir " .. heir_of(ev.player), age = 11 })
        e, reunion = heirs.join(c.heirs, heir_of(ev.player), ctx.past_prologue)
      end
      if reunion then say(c, e.name .. " was away and returns: a reunion") end
      session.welcome(netapi, ev.player, ctx.world_info and ctx.world_info() or {})
      say(c, (e and e.name or "an heir") .. " joins")
    elseif role == "host" and ev.event == "left" then
      local e = heirs.leave(c.heirs, heir_of(ev.player))
      if e then say(c, e.name .. " rests in the pocket home") end
    elseif ev.event == "ended" then
      say(c, "The session ended" .. (ev.error and (": " .. ev.error) or ""))
    end
  end
  if role == "host" then
    if not c.hosting then -- the host's own heir is present
      c.hosting = true
      if c.heirs.list[1].status == "unborn" then
        heirs.create(c.heirs, 1, { sex = "boy", name = "Heir 1", age = 13 })
      end
      heirs.join(c.heirs, 1, ctx.past_prologue)
      hardening.channels(netapi)
    end
    for i = #co.inbox, 1, -1 do
      local m = co.inbox[i]
      if m.channel == "ask" then
        host_asked(c, co, m, ctx) -- (ctx carries netapi for broadcasting votes)
        table.remove(co.inbox, i)
      end
    end
    update_riding(c, co)
    weave.tick(c.weave, 1 / 60)
    if c.ticks % play.CLOCK_EVERY == 0 then
      netapi.send("clock", { time = ctx.now, whole = c.weave.whole, present = heirs.present(c.heirs) })
    end
    if c.vote then
      local r = together.result(c.vote, ctx.in_home or {}, ctx.now)
      if r ~= "open" then
        if r == "passed" and c.vote.kind == "sleep" then
          ctx.sleep(8)
          netapi.send("clock", { time = ctx.now + 8 * 3600, whole = c.weave.whole, present = heirs.present(c.heirs) })
        end
        netapi.send("voted", { kind = c.vote.kind, result = r })
        say(c, "The vote to " .. c.vote.kind .. ": " .. r)
        c.last_vote, c.vote = r, nil
      end
    end
    if c.ticks % play.AREAS_EVERY == 0 and ctx.areas then
      local positions = {}
      for _, p in ipairs(present_players(c)) do
        if co.carpets[p] then positions[p] = co.carpets[p].pos end
      end
      local tier = ctx.tier and ctx.tier() or nil
      local over = tier and tier.budget_ms > 0 and tier.total_ms / tier.budget_ms or 1
      c.areas, c.radius = areas.apply(positions, over, ctx.areas)
    end
  elseif role == "client" then
    for i = #co.inbox, 1, -1 do
      local m = co.inbox[i]
      if m.channel == "clock" then
        if math.abs(m.value.time - ctx.now) > play.DRIFT then ctx.set_time(m.value.time) end
        c.weave.whole = m.value.whole
        table.remove(co.inbox, i)
      elseif m.channel == "voted" then
        say(c, "The vote to " .. m.value.kind .. ": " .. m.value.result)
        c.last_vote, c.open_vote = m.value.result, nil
        table.remove(co.inbox, i)
      elseif m.channel == "vote" then
        c.open_vote = m.value.kind
        say(c, "A vote to " .. m.value.kind .. ": press sleep to agree")
        table.remove(co.inbox, i)
      end
    end
  end
  if role ~= "host" then
    hardening.reconnect(c.reconnect, ctx.now, { status = netapi.status, rejoin = netapi.rejoin })
  end
end

-- Asks the host (or, hosting, does it at once): "split", "rejoin", "vote"
-- (kind) or "ballot" (yes).
function play.ask(c, co, netapi, what, extra)
  local v = { what = what }
  for k, x in pairs(extra or {}) do v[k] = x end
  if co.role == "host" then
    host_asked(c, co, { player = 0, value = v }, { now = extra and extra.now or 0, netapi = netapi })
  else
    netapi.send("ask", v, 0)
  end
end

function play.take_said(c)
  local s = c.said
  c.said = {}
  return s
end

return play
