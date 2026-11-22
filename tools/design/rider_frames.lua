-- SPDX-License-Identifier: MIT
-- Stills from the rider's seat for the UI mockups (tools/design/ui_mockups.py):
-- the eye sits above the back of the carpet, looking ahead, so only the
-- carpet's front edge and tassels are in the picture. Written into
-- ZB_GOLDEN_OUT as rider_<name>.png. Run in a 1920x1080 window.
scenario { name = "rider frames" }
local game = demo.game
game.hud = false

local function settle()
  for _ = 1, 900 do
    step(1)
    local s = world.stream_stats()
    if s.queued == 0 and s.jobs == 0 then return end
    sleep(5)
  end
end

local function ground(p, h)
  local q = world.move(p, 0, 0, 0, 0)
  q.alt = math.max(world.ground_height(q), 0) + (h or 0)
  return q
end

local function bearing(from, to)
  local a, b = world.to_local(from), world.to_local(to)
  return math.atan(b[1] - a[1], -(b[3] - a[3]))
end

game.world_options = { radius = 400000, quality = "quick", seed = 11 }
game.set_mode("create")
for _ = 1, 20000 do
  step(1)
  if game.creation.progress and game.creation.progress.done then break end
  sleep(2)
end
input.inject("confirm", 1, 1)
step(2)
input.inject("confirm", 0, 1)
for _ = 1, 300 do
  step(1)
  if game.mode == "flight" then break end
end
settle()
for _ = 1, 900 do
  step(1)
  if game.world_places.places.ready then break end
  sleep(5)
end
settle()

local c = game.carpet
local start = world.move(c.pos, 0, 0, 0, 0)
local x, y = world.to_cartesian(start)
local NOON = (math.pi - math.atan(y, x)) / (2 * math.pi) % 1 * 86400 + 86400
local plan
for _, p in pairs(game.world_places.places.plans) do
  if p.people and #p.people > 0 then plan = p end
end

-- The game's own view out of the rider's eyes (flight/camera.lua), turned a
-- little down or up for each still.
local camera = require("flight.camera")
local heading
local function rider(pitch)
  camera.reset(game.rider)
  game.rider.pitch = math.rad(-(pitch or 11))
end

local function shot(name, at, h, alt, time, weather, pitch)
  heading = h
  c.heading, c.speed = h, 0
  c.pos = ground(at, alt)
  world.set_time(time)
  sim.set_weather(weather or { wind_east = 1, wind_north = 0, cloud = 0.15, rain = 0, snow = 0, storm = 0 })
  rider(pitch)
  settle()
  step(30)
  render.match_golden("rider_" .. name, "window")
  print("rider frame " .. name)
end

-- Out over the coast by day.
local sea = world.find_water(start, "ocean", 60000)
local b = bearing(start, sea)
local shore = start
for d = 0, world.distance(start, sea), 50 do
  shore = world.move(start, b, d, 0, 0)
  if world.height(shore) < 0 then break end
end
shot("coast", world.move(shore, b, -120, 0, 0), b, 35, NOON + 3 * 3600)
-- Towards the village in the evening.
if plan then
  local centre = plan.settlement.pos
  local from = world.move(centre, 2.2, plan.radius + 80, 0, 0)
  shot("village", from, bearing(from, centre), 14, NOON + 5.6 * 3600, nil, 8)
end
-- Low over the steppe, a wolf pack ahead.
local open = world.move(start, 2, 500, 0, 0)
heading = 0.4
local ahead = world.move(open, heading, 18, 0, 0)
for i, s in ipairs({ { "wolf", 0, -1 }, { "wolf", 1.5, 1 }, { "wolf", 3, -0.5 }, { "deer", 9, 2 } }) do
  sim.spawn_creature(s[1], world.move(ahead, heading, s[2], s[3]))
  if i == 4 then break end
end
shot("wolves", open, heading, 2.5, NOON + 1 * 3600, nil, 6)
-- A grass fire ahead.
local fire = world.move(open, heading, 30, 0, 0)
sim.set_weather({ wind_east = 6, wind_north = 2, cloud = 0.1, rain = 0, snow = 0, storm = 0 })
sim.ignite(fire, 6)
step(60 * 4)
shot("fire", world.move(open, heading, 5, 0, 0), heading, 4, NOON + 1.2 * 3600,
  { wind_east = 6, wind_north = 2, cloud = 0.1, rain = 0, snow = 0, storm = 0 }, 8)
-- Night over the land.
shot("night", world.move(open, heading + 1, 300, 0, 0), heading + 1, 25, NOON + 12 * 3600, nil, -4)
-- A storm.
shot("storm", world.move(open, heading + 2, 600, 0, 0), heading + 2, 20, NOON + 1 * 3600,
  { wind_east = 8, wind_north = 3, cloud = 1, rain = 4, snow = 0, storm = 1 }, 6)
