-- SPDX-License-Identifier: MIT
-- The footage for the social media reels: a world grown from nothing, then
-- shots of it (the carpet, the coast, the mountains, a village, creatures,
-- fire, a storm, the night, the hearth), each a camera move, captured frame
-- by frame (30 frames a second of game time) into ZB_GOLDEN_OUT as
-- reel_00001.png ... Run it in a window of the reel's size (ZB_WINDOW_SIZE=
-- 1080x1920 or 1920x1080); tools/reels/make_reels.py does that and cuts the
-- frames into the videos. It prints where each shot begins.
scenario { name = "reel" }
local game = demo.game
game.hud = false

local EVERY = 1 -- (a preview takes one frame in EVERY)
local frame, shown = 0, 0
step(1)
local size = render.capture("window")
local portrait = size.height > size.width
local FOV = portrait and 78 or 52

local function take()
  frame = frame + 1
  if frame % EVERY == 0 then
    shown = shown + 1
    render.match_golden(string.format("reel_%05d", frame), "window")
  end
end

local function settle(limit)
  for _ = 1, limit or 900 do
    step(1)
    local s = world.stream_stats()
    if s.queued == 0 and s.jobs == 0 then return end
    sleep(5)
  end
end

local cam -- (the camera of the shot being taken; nil: the game's own)

-- Captures a shot of n frames; each frame first calls move(t) with t 0 .. 1.
local function shot(name, n, move)
  print(string.format("reel shot %s from %d", name, frame + 1))
  for i = 1, n do
    move((i - 1) / math.max(1, n - 1))
    game.camera_override = cam
    step(2)
    take()
  end
end

local function ease(t) return t * t * (3 - 2 * t) end

local function ground(p, h)
  local q = world.move(p, 0, 0, 0, 0)
  q.alt = math.max(world.ground_height(q), 0) + (h or 0)
  return q
end

local function view(eye, target, near, far)
  return { eye = world.to_local(eye), target = world.to_local(target), up = { 0, 1, 0 }, fov = FOV,
    near = near or 0.2, far = far or 8000 }
end

-- 0. A world grows: the creation's time-lapse globe.
game.world_options = { radius = 400000, quality = "quick", seed = 11 }
game.set_mode("create")
print(string.format("reel shot globe from %d", frame + 1))
local ticks = 0
for _ = 1, 20000 do
  step(1)
  ticks = ticks + 1
  if ticks % 3 == 0 then take() end
  local p = game.creation.progress
  if p and p.done then break end
  sleep(2)
end
for _ = 1, 30 do -- (the finished world turns a moment)
  step(2)
  take()
end
input.inject("confirm", 1, 1)
step(2)
input.inject("confirm", 0, 1)
for _ = 1, 300 do
  step(1)
  if game.mode == "flight" then break end
end
settle()
for _ = 1, 900 do -- (the places about are built before filming)
  step(1)
  if game.world_places.places.ready then break end
  sleep(5)
end
settle()

local c = game.carpet
local start = world.move(c.pos, 0, 0, 0, 0)
local heading = c.heading
local x, y = world.to_cartesian(start)
local NOON = (math.pi - math.atan(y, x)) / (2 * math.pi) % 1 * 86400 + 86400
local CALM = { wind_east = 1, wind_north = 0, cloud = 0.15, rain = 0, snow = 0, storm = 0 }

-- (the village nearest, chosen now: places are kept only near the heir)
local plan, best
for _, p in pairs(game.world_places.places.plans) do
  local d = world.distance(p.settlement.pos, start)
  if p.people and #p.people > 0 and (not best or d < best) then plan, best = p, d end
end
local outside = world.move(start, 2, (plan and plan.radius or 0) + 300, 0, 0) -- (open land by the village)

local function bearing(from, to)
  local a, b = world.to_local(from), world.to_local(to)
  return math.atan(b[1] - a[1], -(b[3] - a[3]))
end

-- The carpet flies itself along: forward v metres a second, h above the ground.
local function fly(v, h, turn, ease_alt)
  heading = heading + (turn or 0)
  c.heading = heading
  local p = world.move(c.pos, heading, v * 2 / 60, 0, 0)
  local g = math.max(world.ground_height(p), 0)
  p.alt = c.pos.alt + ((g + h) - c.pos.alt) * (ease_alt or 0.08)
  c.pos, c.speed = p, 0
end

-- 1. The carpet: the camera circles it in the late afternoon.
world.set_time(NOON + 4.5 * 3600)
sim.set_weather(CALM)
c.pos = ground(outside, 12)
settle()
shot("carpet", 120, function(t)
  fly(6, 12, 0.003)
  local a = heading + math.pi * (0.7 + 0.7 * ease(t))
  cam = function()
    return view(world.move(c.pos, a, 4.2 - 1.2 * t, 0, 1.1 + 0.8 * t), world.move(c.pos, 0, 0, 0, 0.1))
  end
end)

-- 2. The coast: low and fast out over the sea.
local sea = world.find_water(start, "ocean", 60000)
if sea then
  local b = bearing(start, sea)
  local shore = start
  for d = 0, world.distance(start, sea), 50 do
    shore = world.move(start, b, d, 0, 0)
    if world.height(shore) < 0 then break end
  end
  heading = b
  c.pos = ground(world.move(shore, b, -260, 0, 0), 40)
  world.set_time(NOON + 3 * 3600)
  settle()
  shot("coast", 120, function()
    fly(40, 40, 0)
    cam = function()
      return view(world.move(c.pos, heading + math.pi, 6, 0, 2.5), world.move(c.pos, heading, 200, 0, -45))
    end
  end)
end

-- 4. A village and its people: the camera sweeps round it by day.
if plan then
  local centre = plan.settlement.pos
  world.set_time(NOON - 1.5 * 3600)
  c.pos = ground(centre, 30)
  settle()
  step(120)
  local r = plan.radius + 18
  shot("village", 120, function(t)
    cam = function()
      local e = ground(world.move(centre, 0.4 + 1.1 * ease(t), r, 0, 0), 11 - 4 * t)
      return view(e, ground(centre, 3), 0.3)
    end
  end)
end

-- 5. The creatures: wolves stalk deer, a storm bird circles.
c.pos = ground(outside, 30)
heading = 0
world.set_time(NOON + 0.5 * 3600)
sim.set_weather(CALM)
settle()
local spot = ground(world.move(c.pos, heading, 25, 0, 0))
-- Wolves in the foreground, stalking deer a little ahead.
local placed = { { "wolf", -3, -0.8 }, { "wolf", -4, 0.9 }, { "wolf", -5.5, 0 }, { "deer", 6, -1 }, { "deer", 7, 1 },
  { "deer", 8.5, 0 }, { "storm_bird", 3, 0 } }
local ids = {}
for i, p in ipairs(placed) do ids[i] = sim.spawn_creature(p[1], world.move(spot, heading, p[2], p[3])) end
step(60)
shot("creatures", 105, function(t)
  cam = function()
    local e = ground(world.move(spot, heading, -12 + 2 * ease(t), 0.6 - 1.2 * t, 0), 1.7 + 0.6 * t)
    return view(e, ground(world.move(spot, heading, 4, 0, 0), 0.6), 0.2)
  end
end)
for _, id in ipairs(ids) do pcall(sim.remove_creature, id) end

-- 6. Fire: a grass fire runs before the wind.
local fire
for r = 20, 400, 5 do
  local p = world.move(c.pos, heading, r, 0)
  local cell = sim.fire_at(p)
  if cell.loaded and not cell.water and cell.fuel > 0.2 and cell.moisture < 0.3 then
    fire = p
    break
  end
end
if fire then
  sim.set_weather({ wind_east = 6, wind_north = 2, cloud = 0.1, rain = 0, snow = 0, storm = 0 })
  sim.ignite(fire, 6)
  step(60 * 3)
  shot("fire", 105, function(t)
    cam = function()
      local a = 0.3 + 0.6 * t
      return view(ground(world.move(fire, a, 15 - 4 * t, 0, 0), 5 - 2 * t), ground(fire, 1), 0.3)
    end
  end)
end

-- 7. The night: the fire burns on under the stars.
if fire then
  sim.set_weather({ wind_east = 3, wind_north = 1, cloud = 0, rain = 0, snow = 0, storm = 0 })
  world.set_time(NOON + 12 * 3600)
  step(30)
  shot("night", 105, function(t)
    cam = function()
      local e = ground(world.move(fire, 1.1 + 0.3 * t, 11, 0, 0), 1.8)
      return view(e, ground(fire, 2 + 7 * ease(t)), 0.3)
    end
  end)
end

-- 8. The storm: rain sweeps in and lightning strikes.
heading = 2
c.pos = ground(world.move(outside, heading, 600, 0, 0), 20)
world.set_time(NOON + 1 * 3600)
sim.set_weather({ wind_east = 8, wind_north = 3, cloud = 1, rain = 4, snow = 0, storm = 1 })
settle()
step(60)
local n = 0
shot("storm", 105, function()
  n = n + 1
  if n == 12 or n == 50 or n == 84 then sim.lightning(world.move(c.pos, heading + 0.2 * (n % 3 - 1), 90, 0, 0)) end
  fly(6, 20, 0)
  cam = function()
    return view(world.move(c.pos, heading + math.pi, 5, 1.2, 1.8), world.move(c.pos, heading, 200, 0, 15))
  end
end)

-- 9. Home: inside the tent, by the hearth.
cam = nil
game.camera_override = nil
c.pos = ground(world.move(outside, 0, 0, -40, 0), 1) -- (open land: no tent in a village)
c.heading, c.speed = 0, 0
world.set_time(NOON + 6 * 3600)
sim.set_weather(CALM)
settle()
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
end
press("mount")
press("tent")
if game.tent then
  game.walker.pos = demo.tent.door(game.tent)
  step(2)
  press("mount")
  step(30)
  shot("home", 90, function() end)
end
print(string.format("reel frames %d (%d captured)", frame, shown))
