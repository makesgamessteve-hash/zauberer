-- SPDX-License-Identifier: MIT
-- The M9 exit demo, a client (see together_host.lua): joins by the invite
-- code, makes its heir, rides along, agrees to sleep and reports back.
scenario { name = "together, client" }
local game = demo.game
local co = game.coop
local T = game.together
local coopplay = require("coop.play")
local function wait_for(cond, seconds)
  for _ = 1, seconds * 50 do
    if cond() then return true end
    step(1)
    sleep(20)
  end
  return false
end
local function phase_seen(name)
  for i = #co.inbox, 1, -1 do
    local m = co.inbox[i]
    if m.channel == "demo" and m.value.phase == name then table.remove(co.inbox, i) return true end
  end
  return false
end
expect(net.join_code(together.url, together.code))
expect(wait_for(function() return net.status().role == "client" end, 60), "joined by the code")
expect_eq(net.status().player, together.heir - 1)
net.send("ask", { what = "heir", sex = together.heir % 2 == 0 and "girl" or "boy", name = together.names[together.heir],
  age = 9 + together.heir }, 0)
if together.round == 2 then
  expect(wait_for(function() return phase_seen("again") end, 120))
  net.send("done", { heir = together.heir, time = world.time().time }, 0)
  expect(wait_for(function() return net.status().role == "none" end, 60), "the host ended the session")
  return
end
expect(wait_for(function() return phase_seen("ride") end, 120))
local rode = wait_for(function() return co.riders ~= nil and co.riders.pilot == 0 end, 20)
expect(rode, "riding the Weave with the host as pilot")
expect(wait_for(function() return phase_seen("split") end, 120))
expect(wait_for(function() return phase_seen("reunited") end, 120))
expect(wait_for(function()
  if T.open_vote then
    coopplay.ask(T, co, net, "ballot", { yes = true })
    T.open_vote = nil
  end
  return phase_seen("slept")
end, 120), "agreed to sleep")
expect_eq(T.last_vote, "passed")
for _ = 1, 3 * 60 do step(1) end -- (the host's clock arrives)
net.send("done", { heir = together.heir, time = world.time().time, rode = rode }, 0)
expect(wait_for(function() return net.status().role == "none" end, 60), "the host saved and quit")
