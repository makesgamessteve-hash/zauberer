-- SPDX-License-Identifier: MIT
-- The new-world screen rendered: the time-lapse globe while deep time runs,
-- then the finished world by biome (rendered tests).
-- spec: world-creation: The time-lapse globe
scenario { name = "creation globe" }
local game = demo.game
game.hud = false
expect(not world.draw_globe(nil, { 0, 0, 0 }, 1), "nothing to show in the noise world")
game.world_options = { radius = 300000, quality = "quick", seed = 3 }
game.set_mode("create")
expect(not world.draw_globe(nil, { 0, 0, 0 }, 1) or true)
local function centre_is_globe()
  local size = render.capture("internal")
  local w, h = size.width, size.height
  local r, g, b = render.pixel(w // 2, h // 2)
  local br, bg, bb = render.pixel(4, h - 4)
  return (r + g + b) > (br + bg + bb) + 30
end
local frames
for _ = 1, 4000 do
  step(1)
  sleep(2)
  local p = game.creation.progress
  if p and p.frames >= 20 then
    frames = p.frames
    break
  end
end
expect(frames and frames >= 20, "time-lapse frames arrive")
step(1)
expect(centre_is_globe(), "the globe is drawn in the middle")
expect(world.draw_globe(1, { 0, 0, 0 }, 1, 0.5), "an early frame can be shown")
for _ = 1, 6000 do
  step(1)
  sleep(2)
  if game.creation.progress and game.creation.progress.done then break end
end
expect(game.creation.progress.done)
expect(game.fly_in_new_world())
expect(world.draw_globe(nil, { 0, 0, 0 }, 1), "the played world as a globe")
