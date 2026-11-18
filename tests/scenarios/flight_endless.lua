-- SPDX-License-Identifier: MIT
-- The M1 exit demo, headless: ten minutes of flight at full boost, north, east,
-- south and west, never below the ground, with terrain streamed in behind.
-- spec: carpet-flight: Ten minutes in any direction
scenario { name = "ten minutes of flight" }
world.view_distance(768, 3000) -- smaller rings keep the headless run quick
local c = demo.game.carpet
local carpet = demo.carpet
expect(world.height(c.pos) > 0, "the flight starts over land")
local start = { face = c.pos.face, x = c.pos.x, y = c.pos.y }
local headings = { 0, math.pi / 2, math.pi, -math.pi / 2 }
local low = math.huge
for leg = 1, 4 do
  c.heading = headings[leg]
  for _ = 1, 150 do -- 150 s per leg
    input.inject("steer", 0, 1, 60)
    input.inject("boost", 1, 0, 60)
    step(60)
    local clearance = c.pos.alt - math.max(world.height(c.pos), 0)
    low = math.min(low, clearance)
    expect(c.pos.alt == c.pos.alt and c.speed == c.speed, "no NaN in the flight state")
  end
end
expect(low >= carpet.CLEARANCE - 0.01, "never below the ground: lowest clearance " .. low)
expect(c.distance > 4 * 140 * 145, "flew about 85 km: " .. math.floor(c.distance))
expect_eq(engine.tick(), 36000)
-- Brake to a stop; the streamed terrain then catches up with the carpet.
input.inject("steer", 0, -1, 300)
step(300)
expect_eq(c.speed, 0, "braked to a stop")
local ready = false
for _ = 1, 6000 do
  step(1)
  if world.chunk_stage(c.pos) >= 2 then
    ready = true
    break
  end
end
expect(ready, "the chunk under the carpet is generated")
expect(world.stream_stats().evicted >= 0)
print(string.format("flew %.1f km from chunk %d/%d/%d to %d/%d/%d", c.distance / 1000, start.face, start.x,
  start.y, c.pos.face, c.pos.x, c.pos.y))
