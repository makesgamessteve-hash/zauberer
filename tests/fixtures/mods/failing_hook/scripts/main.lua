-- SPDX-License-Identifier: MIT
local ticks = 0
engine.on("tick", function()
  ticks = ticks + 1
  _G.failing_hook_ticks = ticks
  if ticks == 5 then
    error("failing_hook fixture: deliberate error at tick 5")
  end
end)
