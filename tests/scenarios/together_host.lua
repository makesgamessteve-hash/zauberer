-- SPDX-License-Identifier: MIT
-- The M9 exit demo, the host (tests/engine/test_together.py starts it with a
-- relay and three clients, `together` set by the test): four heirs join one
-- world by invite code, ride the whole Weave together, split up into two
-- regions (two active areas), reunite and rejoin the Weave, pitch the tent
-- and vote to sleep; the host saves and quits. On the next day (round 2)
-- the host plays the saved world again and everyone rejoins.
-- spec: coop-sessions: The M9 exit demo
-- spec: accessibility: Escape opens the menu in play
scenario { name = "together, host" }
local game = demo.game
local co = game.coop
local T = game.together
local coopplay = require("coop.play")
local heirs = require("coop.heirs")
local areas = require("coop.areas")
local function wait_for(cond, seconds)
  for _ = 1, seconds * 50 do
    if cond() then return true end
    step(1)
    sleep(20)
  end
  return false
end
local ok, err = net.host_relay(together.url)
expect(ok, err)
expect(wait_for(function() return net.status().invite_code ~= nil end, 30), "an invite code from the relay")
print("invite code: " .. net.status().invite_code)
if together.round == 2 then -- (before anyone is back: from the save)
  for i = 2, 4 do expect_eq(T.heirs.list[i].name, together.names[i], "the heirs came back with the save") end
  expect(world.time().time >= together.slept_after, "the world went on from where it was left")
end
expect(wait_for(function() return #net.status().players == 4 end, 180), "three heirs joined by the code")
expect(wait_for(function()
  for i = 2, 4 do
    if T.heirs.list[i].name ~= together.names[i] then return false end
  end
  return true
end, 60), "each player's heir as they made it")
expect_eq(#heirs.present(T.heirs), 4)
-- In company the menu does not hold the world still.
input.inject("quit", 1, 1)
step(2)
expect(game.menu.open, "the menu is open")
local menu_at = world.time().time
step(10)
expect(world.time().time > menu_at, "in company the world goes on with the menu open")
input.inject("quit", 1, 1)
step(2)
expect(not game.menu.open)
local function phase(name) net.send("demo", { phase = name }) end

if together.round == 2 then
  phase("again")
  local reports = 0
  expect(wait_for(function()
    for i = #co.inbox, 1, -1 do
      if co.inbox[i].channel == "done" then reports = reports + 1; table.remove(co.inbox, i) end
    end
    return reports == 3
  end, 120), "everyone is back")
  step(30)
  net.leave()
  return
end

-- 1. Riding the whole Weave together: the host pilots, the others ride along.
phase("ride")
game.carpet.speed = 20
expect(wait_for(function() return co.riders ~= nil and co.carpets[3] ~= nil end, 20))
for _ = 1, 120 do step(1) end
for p = 1, 3 do
  expect(world.distance(co.carpets[p].pos, co.carpets[0].pos) < 6, "heir " .. p .. " rides the Weave")
end
-- 2. Splitting the Weave; two heirs explore another region.
coopplay.ask(T, co, net, "split", { now = world.time().time })
for _ = 1, 4 do step(1) end
expect(not T.weave.whole and co.riders == nil, "one carpet each")
phase("split")
local far = world.move(co.carpets[0].pos, math.pi, 6000, 0, 0)
for p = 2, 3 do
  co.carpets[p].pos = world.move(far, 0, 0, p * 5, 0)
  co.carpets[p].speed = 0
end
game.carpet.speed = 0
for _ = 1, coopplay.AREAS_EVERY * 2 do step(1) end
expect(T.areas and #T.areas == 2, "an active area for each group")
expect(T.radius < areas.RADIUS, "sharing the budget")
-- 3. Reuniting: together again, the Weave rejoins.
for p = 1, 3 do
  co.carpets[p].pos = world.move(co.carpets[0].pos, 0, 0, p * 3, 0)
  co.carpets[p].speed = 0
end
for _ = 1, 3 * 60 do step(1) end
coopplay.ask(T, co, net, "rejoin", { now = world.time().time })
expect(T.weave.whole, "the Weave is whole again")
phase("reunited")
-- 4. The tent, from the whole Weave.
local here = world.move(game.carpet.pos, 0, 0, 0, 0)
here.alt = world.ground_height(here)
local pitched = demo.tent.pitch(here, 0)
expect(pitched and require("coop.weave").can_pitch(T.weave), "the tent is pitched")
game.tent = pitched
-- 5. A vote to sleep: everyone agrees, the night passes for all.
local before = world.time().time
coopplay.ask(T, co, net, "vote", { kind = "sleep", now = before })
expect(wait_for(function() return T.last_vote ~= nil end, 60), "the vote is decided")
expect_eq(T.last_vote, "passed")
expect(world.time().time >= before + 8 * 3600 - 60, "the night has passed")
phase("slept")
local reports = {}
expect(wait_for(function()
  for i = #co.inbox, 1, -1 do
    local m = co.inbox[i]
    if m.channel == "done" then reports[#reports + 1] = m.value; table.remove(co.inbox, i) end
  end
  return #reports == 3
end, 60), "three reports")
for _, r in ipairs(reports) do
  expect(r.rode, "rode the Weave: heir " .. tostring(r.heir))
  expect(math.abs(r.time - world.time().time) < 120, "on the host's clock")
end
print("slept until: " .. math.floor(world.time().time))
-- 6. The host saves and quits; the session ends for everyone.
game.save()
step(5)
net.leave()
