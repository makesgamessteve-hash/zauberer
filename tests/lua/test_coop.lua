-- SPDX-License-Identifier: MIT
-- Unit tests of the co-op game layer (coop/heirs, weave, together, areas,
-- hardening, session).
local heirs = require("coop.heirs")
local weave = require("coop.weave")
local together = require("coop.together")
local areas = require("coop.areas")
local hardening = require("coop.hardening")
local session = require("coop.session")

-- spec: heirs: One to four heirs
test.case("the heirs: made by their players, twins, the count set by the host", function()
  local h = heirs.new(3)
  test.expect(h.count == 3 and #h.list == 3 and h.list[1].status == "unborn")
  test.expect_eq(heirs.new(9).count, 4)
  test.expect_eq(heirs.new(0).count, 1)
  test.expect(heirs.create(h, 1, { sex = "girl", name = "Shirin", age = 12 }))
  test.expect_eq(select(2, heirs.create(h, 2, { sex = "cat", name = "x", age = 12 })), "a girl or a boy")
  test.expect_eq(select(2, heirs.create(h, 2, { sex = "boy", name = "", age = 12 })), "a name of 1 to 20 letters")
  test.expect_eq(select(2, heirs.create(h, 2, { sex = "boy", name = "Dara", age = 16 })), "an age of 9 to 14")
  test.expect_eq(select(2, heirs.create(h, 4, { sex = "boy", name = "Dara", age = 10 })), "there are 3 heirs")
  test.expect_eq(select(2, heirs.create(h, 2, { sex = "boy", name = "Dara", twin = 3 })),
    "no such sibling to be a twin of")
  local twin = heirs.create(h, 2, { sex = "boy", name = "Dara", twin = 1, age = 9 })
  test.expect(twin.age == 12, "twins share their age")
  local back = heirs.new(1, heirs.snapshot(h))
  test.expect(back.count == 3 and back.list[1].name == "Shirin")
  heirs.age(h, 2)
  test.expect_eq(h.list[1].age, 14)
end)

-- spec: heirs: Present and absent heirs
test.case("joining and leaving: absent heirs rest in the home; latecomers are met at a reunion", function()
  local h = heirs.new(3)
  heirs.create(h, 1, { sex = "girl", name = "Shirin", age = 12 })
  heirs.create(h, 2, { sex = "boy", name = "Dara", age = 10 })
  test.expect_eq(select(2, heirs.join(h, 3, false)), "this heir is not made yet")
  local e, reunion = heirs.join(h, 1, false)
  test.expect(e.status == "present" and not reunion and e.seen_prologue)
  local late, owed = heirs.join(h, 2, true)
  test.expect(late and owed and late.away, "away when Anarvan burned")
  heirs.reunite(h, 2)
  test.expect(h.list[2].reunited and not h.list[2].away)
  test.expect_eq(#heirs.present(h), 2)
  local gone = heirs.leave(h, 2)
  test.expect(gone.status == "resting" and gone.doing, "resting in the pocket home")
  test.expect(heirs.leave(h, 2) == nil)
  test.expect_eq(#heirs.present(h), 1)
  local again = heirs.new(3, heirs.snapshot(h))
  test.expect_eq(again.list[1].status, "resting") -- (everyone rests until they join again)
  local shared, personal = heirs.split({ world = 1, health = 2, codex = 3, spellbook = 4 })
  test.expect(shared.world == 1 and shared.codex == 3 and personal.health == 2 and personal.spellbook == 4)
  test.expect(shared.health == nil and personal.world == nil)
  local f1, f2 = heirs.fragments(1, 2, 5), heirs.fragments(2, 2, 5)
  test.expect(#f1 + #f2 == 5 and f1[1] ~= f2[1], "each sees other fragments")
end)

-- spec: weave-splitting: Riding together
test.case("riding the whole Weave: one pilots, the others cast; full strength", function()
  local W = weave.new()
  local pilot, casters = weave.ride(W, { 1, 2, 3 })
  test.expect(pilot == 1 and #casters == 2)
  pilot = weave.ride(W, { 2, 3 }, 3)
  test.expect_eq(pilot, 3)
  test.expect_eq(weave.ride(W, { 2 }), 2) -- (the pilot gone, another takes the reins)
  local base = { lift = 10, speed = 40, ceiling = 2000, reservoir = 100 }
  local s = weave.stats(W, base, 1, 1)
  test.expect(s.lift == 10 and s.speed == 40 and s.reservoir == 100)
end)

-- spec: weave-splitting: Splitting and rejoining
test.case("splitting into weaker pieces, rejoining close together, damage carried over", function()
  local W = weave.new()
  test.expect_eq(select(2, weave.split(W, { 1 })), "one heir rides the whole Weave")
  local pieces = weave.split(W, { 1, 2 })
  test.expect(pieces[1] and pieces[2] and pieces[1].cloak and not W.whole)
  test.expect_eq(select(2, weave.split(W, { 1, 2 })), "the Weave is already unravelled")
  local base = { lift = 10, speed = 40, ceiling = 2000, reservoir = 100 }
  local strong, weak = weave.stats(W, base, 1, 1), weave.stats(W, base, 2, 0)
  test.expect(strong.lift < 10 and strong.reservoir == 50 and weak.lift < strong.lift, "weaker, by the bond")
  weave.hurt(W, 1, 5)
  weave.hurt(W, 2, 3)
  test.expect(not weave.can_pitch(W), "the tent needs the whole Weave")
  local far = function() return 50 end
  test.expect_eq(select(2, weave.rejoin(W, { [1] = 1, [2] = 2 }, far)), "come closer together to rejoin")
  test.expect(weave.rejoin(W, { [1] = 1, [2] = 2 }, function() return 5 end))
  test.expect(W.whole and W.damage == 8 and W.busy == weave.SPLIT_TIME)
  test.expect_eq(select(2, weave.rejoin(W, {}, far)), "the Weave is whole")
  test.expect_eq(select(2, weave.split(W, { 1, 2 })), "the Weave is still moving")
  weave.tick(W, 3)
  weave.hurt(W, 1, 1)
  test.expect_eq(W.damage, 9)
  weave.upgrade(W, "homing")
  weave.split(W, { 1, 2 })
  test.expect(weave.has(W, "homing") and weave.can_pitch(W), "Homing brings a piece back to the tent")
  local back = weave.new(weave.snapshot(W))
  test.expect(not back.whole and back.upgrades.homing)
end)

-- spec: coop-time-and-scaling: Votes on time
test.case("sleep needs everyone in the home or a vote; anyone can ask to wait", function()
  local v = together.vote("sleep", 1, { 1, 2, 3 }, 0)
  test.expect_eq(together.result(v, {}, 1), "open")
  test.expect_eq(together.result(v, { [2] = true, [3] = true }, 1), "passed")
  together.cast(v, 2, true)
  together.cast(v, 3, true)
  test.expect_eq(together.result(v, {}, 1), "passed")
  together.cast(v, 3, false)
  test.expect_eq(together.result(v, {}, 1), "refused")
  local w = together.vote("sleep", 1, { 1, 2 }, 0)
  test.expect_eq(together.result(w, {}, together.VOTE_TIME + 1), "expired")
end)

-- spec: coop-time-and-scaling: Rituals, scaling, friendly fire and revival
test.case("joint rituals, the world's answer, friendly fire, revival", function()
  local d1, m1 = together.ritual(60, 40, 1)
  local d3, m3 = together.ritual(60, 40, 3)
  test.expect(d1 == 60 and d3 == 30 and m3 < m1, "together faster, each giving less")
  local one, four = together.scale(1), together.scale(4)
  test.expect(four.group > one.group and four.boldness > one.boldness and four.mage > one.mage)
  test.expect(four.resources == 1 and one.resources == 1, "resources do not scale")
  test.expect_eq(together.scale(9).group, together.scale(4).group)
  test.expect(together.harm(10, 1, 2, false) == 10 and together.harm(10, 1, 2, true) == 2.5)
  test.expect_eq(together.harm(10, 1, 1, true), 10)
  local d = together.down(2, 100)
  test.expect_eq(select(2, together.revive(d, 2, "medicine", 110)), "one cannot revive oneself")
  test.expect_eq(select(2, together.revive(d, 1, "a song", 110)), "it takes life magic or medicine")
  test.expect(not together.carried(d, 120))
  test.expect(together.revive(d, 1, "life magic", 120) and d.revived == 1)
  local late = together.down(3, 0)
  test.expect(select(2, together.revive(late, 1, "medicine", 100)):find("too late", 1, true))
  test.expect(together.carried(late, 100))
end)

-- spec: multi-area-simulation: An area for each group of heirs
test.case("heirs together share an area, heirs apart get one each, radii shrink", function()
  local dist = function(a, b) return math.abs(a - b) end
  local list = areas.cluster({ [0] = 0, [1] = 500, [2] = 10000, [3] = 10300 }, dist)
  test.expect(#list == 2 and #list[1].members == 2 and #list[2].members == 2)
  local five = areas.cluster({ a = 0, b = 1e5, c = 2e5, d = 3e5, e = 4e5 }, dist)
  test.expect_eq(#five, areas.MAX)
  test.expect(areas.radius(1) == areas.RADIUS and areas.radius(4) < areas.radius(2))
  test.expect(areas.radius(4, 3) >= areas.FLOOR and areas.radius(4, 3) < areas.radius(4))
  local focus, cleared, radius = {}, {}, nil
  local got, r = areas.apply({ [0] = 0, [1] = 50000 }, 1, { distance = dist,
    set_focus = function(i, p) focus[i] = p end, clear_focus = function(i) cleared[i] = true end,
    set_radius = function(x) radius = x end })
  test.expect(#got == 2 and focus[1] == 0 and focus[2] == 50000 and cleared[3] and cleared[4] and radius == r)
end)

-- spec: network-hardening: Smooth over latency
test.case("others are drawn between snapshots in the past", function()
  local buf = hardening.buffer()
  test.expect(hardening.sample(buf, 1, nil) == nil)
  for k = 0, 40 do hardening.push(buf, k * 0.05, k) end
  local lerp = function(a, b, f) return a + (b - a) * f end
  test.expect(#buf.list == 32)
  test.expect_near(hardening.sample(buf, 1.525, lerp), 30.5, 1e-9)
  test.expect_eq(hardening.sample(buf, 0, lerp), buf.list[1].s)
  test.expect_eq(hardening.sample(buf, 99, lerp), 40)
end)

-- spec: network-hardening: Rejoining after a lost connection
test.case("a lost connection is tried again with growing waits within the grace", function()
  local R, tries = {}, 0
  local status = { role = "none", can_rejoin = true }
  local api = { status = function() return status end, rejoin = function() tries = tries + 1 end }
  test.expect_eq(hardening.reconnect(R, 0, api), "trying")
  test.expect_eq(hardening.reconnect(R, 0.2, api), "waiting")
  test.expect_eq(hardening.reconnect(R, 1.1, api), "trying")
  test.expect_eq(hardening.reconnect(R, 3.2, api), "trying")
  test.expect(tries == 3)
  test.expect_eq(hardening.reconnect(R, 40, api), "gave up")
  status = { role = "client" }
  test.expect_eq(hardening.reconnect(R, 41, api), "connected")
  status = { role = "none" }
  test.expect_eq(hardening.reconnect({}, 42, api), "idle")
end)

-- spec: network-hardening: What is sent, and checked
test.case("entities near an heir first; state hashed and differences found", function()
  local dist = function(a, b) return math.abs(a - b) end
  local list = hardening.interest(0, { a = { pos = 100 }, b = { pos = 50 }, c = { pos = 9999 } }, dist)
  test.expect(#list == 2 and list[1].id == "b")
  local h = function(v) return engine.hash_state(v) end
  local mine = hardening.hashes({ carpets = { 1, 2 }, time = 5 }, h)
  local theirs = hardening.hashes({ carpets = { 1, 3 }, time = 5 }, h)
  local bad = hardening.desync(mine, theirs)
  test.expect(#bad == 1 and bad[1] == "carpets")
  local set = {}
  hardening.channels({ set_channel = function(ch, class) set[ch] = class end })
  test.expect(set.state == "latest" and set.deltas == "reliable" and set.input == "reliable")
end)

-- A fake session layer for the session's logic.
local function fake_net()
  local f = { sent = {}, blobs = {}, cache = {}, status_v = { role = "none" } }
  f.host_relay = function(url)
    if url == "bad" then return nil, "relay refused" end
    f.status_v = { role = "host" }
    return true
  end
  f.host = function() f.status_v = { role = "host" } return 1234 end
  f.lan_announce = function(on) f.announced = on return true end
  f.join_code = function(_, code) if code == "nope" then return nil, "unknown code" end return true end
  f.join = function() return true end
  f.status = function() return f.status_v end
  f.send = function(ch, v, to) f.sent[#f.sent + 1] = { ch = ch, v = v, to = to } end
  f.send_blob = function(to, name, data) f.blobs[#f.blobs + 1] = { to = to, name = name, data = data } end
  f.blob_hash = function(d) return engine.hash_state(d) end
  f.blob_cached = function(hh) return f.cache[hh] ~= nil end
  f.blob_load = function(hh) return f.cache[hh] end
  f.blob_store = function(d) f.cache[f.blob_hash(d)] = d return f.blob_hash(d) end
  return f
end

-- spec: coop-sessions: Hosting and joining
test.case("hosting by relay or LAN, joining by code or address, refusals", function()
  local f = fake_net()
  local S = session.new()
  test.expect(not session.host(S, f, "relay", { url = "bad" }) and S.error == "relay refused")
  test.expect(session.host(S, f, "relay") and S.phase == "hosting")
  f.status_v = { role = "host", invite_code = "K7QX4M" }
  session.tick(S, f, {})
  test.expect(S.code == "K7QX4M" and session.line(S) == "Invite code: K7QX4M")
  local L = session.new()
  test.expect(session.host(L, f, "lan") and f.announced)
  local C = session.new()
  test.expect(not session.join(C, f, "code", "nope") and session.line(C) == "Co-op: unknown code")
  test.expect(session.join(C, f, "code", "K7QX4M") and C.phase == "joining")
  f.status_v = { role = "client" }
  test.expect_eq(session.tick(C, f, {})[1].what, "joined")
  local ev = session.tick(C, f, { { event = "ended", error = "packages differ", mismatches = { { id = "x" } } } })
  test.expect(ev[1].what == "ended" and C.mismatches[1].id == "x" and C.phase == "alone")
  local A = session.new()
  test.expect(session.join(A, f, "address", "ws://127.0.0.1:1"))
  test.expect(session.line(session.new()) == nil)
end)

-- spec: coop-sessions: The world sent to joiners
test.case("the host sends the world; the joiner keeps the bundle in its cache", function()
  local f = fake_net()
  session.welcome(f, 2, { seed = 7, name = "w", bundle = "BUNDLEBYTES", history = "HISTORY" })
  test.expect(f.sent[1].ch == "world" and f.sent[1].to == 2 and f.sent[1].v.bundle and f.sent[1].v.history)
  test.expect(#f.blobs == 2 and f.blobs[1].name == "bundle")
  local C = session.new()
  local ev = session.tick(C, f, { { event = "data", channel = "world", value = f.sent[1].v } })
  test.expect(ev[1].what == "world" and C.waiting.bundle and C.waiting.history)
  session.tick(C, f, { { event = "blob_progress", name = "bundle", received = 5, total = 10 } })
  test.expect_eq(session.line(C), "Receiving the world: 50%")
  session.tick(C, f, { { event = "blob", name = "bundle", data = "BUNDLEBYTES" } })
  ev = session.tick(C, f, { { event = "blob", name = "history", data = "HISTORY" } })
  test.expect(ev[1].what == "ready" and C.bundle == "BUNDLEBYTES" and C.history == "HISTORY")
  -- A second join with the bundle cached waits for nothing.
  local D = session.new()
  ev = session.tick(D, f, { { event = "data", channel = "world", value = { seed = 7, bundle = true,
    hash = f.blob_hash("BUNDLEBYTES") } } })
  test.expect(D.bundle == "BUNDLEBYTES" and ev[#ev].what == "ready")
  ev = session.tick(D, f, { { event = "joined", player = 1 }, { event = "left", player = 1 } })
  test.expect(#ev == 1 and ev[1].what == "left")
end)
