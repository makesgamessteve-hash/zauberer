-- SPDX-License-Identifier: MIT
-- Input actions fly the carpet.
-- spec: carpet-flight: Flying from input
-- spec: touch-and-gamepad-controls: Same actions on every device
scenario { name = "controls fly the carpet" }
local c = demo.game.carpet
expect_eq(demo.game.mode, "flight")
expect_eq(c.speed, 0, "the carpet starts hovering")
input.inject("steer", 0, 1, 120)
step(120)
expect(c.speed > 30, "throttle accelerates")
local heading = c.heading
input.inject("steer", 1, 0, 60)
step(60)
expect(c.heading > heading, "steering right turns clockwise")
local alt = c.pos.alt
input.inject("climb", 1, 0, 120)
step(120)
expect(c.pos.alt > alt + 20, "climbing gains height")
input.inject("boost", 1, 0, 300)
step(300)
expect(c.speed > 140, "boost")
input.inject("climb_touch", 0, -1, 60)
local high = c.pos.alt
step(60)
expect(c.pos.alt < high, "the touch climb stick descends")
input.inject("chime")
step(1)
input.inject("credits")
step(1)
expect(demo.game.state.credits, "credits open in flight")
input.inject("look", 1, 0, 30)
step(30)
-- The model may still be loading behind the terrain jobs on a small machine.
for _ = 1, 1000 do
  if render.state(demo.assets.carpet) ~= "pending" then break end
  step(1)
  sleep(5)
end
expect_eq(render.state(demo.assets.carpet), "ready")
step(2) -- frames draw the HUD and the carpet
expect(render.stats().model_draws >= 1, "the carpet is drawn")
local local_pos = world.to_local(c.pos)
expect(math.abs(local_pos[1]) < 1100 and math.abs(local_pos[3]) < 1100, "the render origin follows the carpet")
local back = world.from_local(local_pos[1], local_pos[2], local_pos[3])
expect_eq(back.face, c.pos.face)
