-- SPDX-License-Identifier: MIT
-- Co-op, host side (run with --host 0; a second process joins with coop_client.lua).
-- spec: netcode: Host and client
-- spec: netcode: Host authority
scenario { name = "co-op host" }
local co = demo.game.coop
expect_eq(net.status().role, "host")
expect_eq(net.status().player, 0)
local function wait_for(cond, seconds)
  for _ = 1, seconds * 100 do
    if cond() then return true end
    step(1)
    sleep(10)
  end
  return false
end
expect(wait_for(function() return #net.status().players == 2 end, 90), "a client joined")
expect(co.carpets[1] ~= nil, "the joining heir got a carpet")
local start = co.carpets[1].distance
-- The client flies; its report arrives on the "done" channel.
local report
expect(wait_for(function()
  for _, m in ipairs(co.inbox) do
    if m.channel == "done" then report = m.value return true end
  end
  return false
end, 60), "the client reported back")
expect(co.carpets[1].distance > start + 100, "the host moved the client's carpet from its inputs")
expect(report and report.mispredictions == 0, "no mispredictions on the client")
step(30)
net.leave()
expect_eq(net.status().role, "none")
