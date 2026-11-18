-- SPDX-License-Identifier: MIT
-- Co-op, client side (run with --join ws://...; coop_host.lua hosts).
-- spec: netcode: Prediction matches the host
-- spec: netcode: Same terrain on both sides
-- spec: netcode: Player indices
scenario { name = "co-op client" }
local co = demo.game.coop
local function wait_for(cond, seconds)
  for _ = 1, seconds * 100 do
    if cond() then return true end
    step(1)
    sleep(10)
  end
  return false
end
expect(wait_for(function() return net.status().role == "client" end, 30), "joined: " .. net.status().error)
expect_eq(net.status().player, 1, "the first client is player 1")
expect(wait_for(function() return co.carpets[1] ~= nil and co.carpets[0] ~= nil end, 30), "both carpets known")
for _ = 1, 300 do
  input.inject("steer", 0.3, 1, 1)
  step(1)
  sleep(2)
end
expect(wait_for(function() return co.predicted_ok >= 20 and co.terrain_checks >= 1 end, 60),
  "predictions checked and terrain compared")
expect_eq(co.mispredictions, 0, "every checked prediction equalled the host's state")
expect_eq(co.terrain_mismatches, 0, "the same terrain on both sides")
expect(net.status().rtt_ms ~= nil)
net.send("done", { mispredictions = co.mispredictions, predicted_ok = co.predicted_ok }, 0)
expect(wait_for(function() return net.status().role == "none" end, 30), "the host ended the session")
expect_eq(net.status().error, "the host ended the session")
