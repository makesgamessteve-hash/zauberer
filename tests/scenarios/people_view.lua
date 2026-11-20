-- SPDX-License-Identifier: MIT
-- Rendered: the people of a place stand on the ground and are seen. The same
-- view is taken with three villagers on open grass before the camera and with
-- them gone; the pixels around their feet must differ (rendered tests).
-- spec: npcs: People are seen
scenario { name = "people view" }
local game = demo.game
game.hud = false
render.daylight(false)
for _ = 1, 900 do
  step(1)
  local s = world.stream_stats()
  if s.queued == 0 and s.jobs == 0 and game.world_places.places.ready then break end
  sleep(5)
end
step(60)
local P = game.world_places.places
local plan
for _, p in pairs(P.plans) do plan = p end
expect(plan and plan.people and #plan.people >= 3, "the village is peopled")
-- Open grass beyond the village; the carpet waits behind the camera (people are drawn near the heir).
local spot = world.move(plan.settlement.pos, 0, 0, -(plan.radius + 25), 0)
spot.alt = world.height(spot)
game.carpet.pos = world.move(spot, 0, 0, -20, 3)
local function place(away)
  for i = 1, 3 do
    local q = plan.people[i]
    local at = world.move(spot, 0, i * 1.5 - 3, away and 500 or 0, 0)
    q.pos, q.goal = at, world.move(at, 0, 0, 0, 0)
  end
end
game.camera_override = function()
  return { eye = world.to_local(world.move(spot, 0, 0, -6, 1.6)),
    target = world.to_local(world.move(spot, 0, 0, 0, 0.9)), up = { 0, 1, 0 }, fov = 60, near = 0.1, far = 3000 }
end
render.pin_time(1000)
local function sample()
  local size = render.capture("internal")
  local px = {}
  for y = size.height * 2 // 5, size.height * 4 // 5, 2 do
    for x = size.width * 2 // 5, size.width * 3 // 5, 2 do
      local r, g, b = render.pixel(x, y)
      px[#px + 1] = r * 65536 + g * 256 + b
    end
  end
  return px
end
place(false)
step(3)
local with = sample()
place(true)
step(3)
local without = sample()
local changed = 0
for i = 1, #with do changed = changed + (with[i] ~= without[i] and 1 or 0) end
expect(changed > #with // 10, "the villagers are seen (" .. changed .. " of " .. #with .. " pixels)")
render.pin_time(nil)
