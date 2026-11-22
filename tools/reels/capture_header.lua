-- SPDX-License-Identifier: MIT
-- Put before a scenario (tools/reels/gameplay.py does): every other tick the
-- scenario steps is captured into ZB_GOLDEN_OUT as play_00001.png ..., so a
-- scripted playthrough becomes footage. Long waits in one step(n) become a
-- time-lapse of a few seconds; ticks stepped while the world is still
-- loading (between calls of sleep) are not captured; past every 900 frames
-- the capture rate halves, so a long playthrough stays a watchable length.
local reel_step, reel_sleep = step, sleep
local reel = { frame = 0, tick = 0, stride = 2, since_sleep = 99 }

local function reel_take()
  reel.frame = reel.frame + 1
  render.match_golden(string.format("play_%05d", reel.frame), "window")
  if reel.frame % 900 == 0 then reel.stride = reel.stride * 2 end
end

function sleep(ms) -- luacheck: ignore 121
  reel.since_sleep = 0
  return reel_sleep(ms)
end

function step(n) -- luacheck: ignore 121
  n = n or 1
  reel.since_sleep = reel.since_sleep + 1
  local loading = reel.since_sleep < 3
  if n <= 240 then
    for _ = 1, n do
      reel_step(1)
      reel.tick = reel.tick + 1
      if not loading and reel.tick % reel.stride == 0 then reel_take() end
    end
  else -- (a long wait: a time-lapse of about three seconds)
    local shots = 90
    local each = n // shots
    for k = 1, shots do
      reel_step(k < shots and each or n - each * (shots - 1))
      reel.tick = reel.tick + each
      if not loading then reel_take() end
    end
  end
end
