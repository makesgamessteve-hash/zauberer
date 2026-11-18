-- SPDX-License-Identifier: MIT
-- Entry script of the base package. Three modes:
--  * "flight" (default, M1 First Flight): a carpet over the streamed planet,
--  * "create" (M2 A Planet): a new world made while its continents drift on a
--    turning globe, then flown in,
--  * "showcase" (the M0 demo): the carpet seen from an orbiting camera, with
--    mana motes, text, sound and input from every device.
local state = require("demo.state")
local hud = require("demo.hud")
local touch = require("demo.touch")
local carpet = require("flight.carpet")
local camera = require("flight.camera")
local controls = require("flight.controls")
local flight_hud = require("flight.hud")
local spawn = require("flight.spawn")
local coop = require("flight.coop")
local creation = require("create.screen")
local magic = require("magic.play")
local fauna = require("life.fauna")

local game = { mode = "flight", state = state.new(), fullscreen = false, hud = true, save_name = "default" }
local AUTOSAVE_TICKS = 5 * 60 * 60 -- every five minutes of flight
local assets = {}

local SHOWCASE_ACTIONS = { "turn", "look", "zoom", "spin", "chime", "credits", "fullscreen", "quit" }
local WORLD_START = 8 * 3600 -- a new world's time (game seconds)
local WORLD = { move = world.move, height = world.height, water = world.water, chunk_hash = world.chunk_hash }
local SUN = { 0.45, 0.6, -0.66 }

-- A new world begins on a spring morning where the carpet starts (its clock
-- runs at longitude 0): 08:00 local solar time on the first day.
local function morning_at(pos)
  local x, y = world.to_cartesian(pos)
  local noon = (math.pi - math.atan(y, x)) / (2 * math.pi)
  return ((noon - 4 / 24) % 1) * 86400
end

local function setup_flight()
  local p = world.planet()
  local pos, heading = spawn.find(world.height, p.chunks_per_edge), 0
  pos.alt = math.max(world.height(pos), 0) + 60
  local back = game.restore -- the player of a loaded save
  if back and type(back.pos) == "table" then
    pos, heading = back.pos, back.heading or 0
  end
  game.restore = nil
  if world.time().time == WORLD_START then
    world.set_time(morning_at(pos))
  end
  game.carpet = carpet.new(pos, heading)
  game.magic = magic.new(back and back.magic)
  if game.fauna then
    fauna.clear(game.fauna)
  end
  game.fauna = fauna.new(pos, heading)
  game.chase = camera.new()
  game.coop = coop.new()
  game.last_frame_time = nil
  render.light({ direction = { -SUN[1], -SUN[2], -SUN[3] }, color = { 1, 0.95, 0.85 }, ambient = { 0.38, 0.4, 0.48 } })
  render.atmosphere({ zenith = { 0.2, 0.4, 0.78 }, horizon = { 0.72, 0.8, 0.88 }, sun = SUN,
    sun_color = { 1, 0.95, 0.8 }, fog_distance = 12000 })
  render.palette({})
  render.daylight(true) -- the sun, the moon and the seasons over the carpet
  world.set_focus(1, game.carpet.pos, { 0, 0, 0 }, carpet.forward(game.carpet))
end

local function setup_create()
  world.clear_focus(1)
  render.daylight(false)
  render.atmosphere(false)
  render.palette({})
  render.clear_color(0.02, 0.02, 0.06)
  render.light({ direction = { -0.5, -0.3, -0.8 }, color = { 1, 0.95, 0.85 }, ambient = { 0.25, 0.25, 0.32 } })
  local options = game.world_options or {}
  options.seed = options.seed or engine.info().seed
  game.creation = creation.new(options)
end

local function setup_showcase()
  world.clear_focus(1)
  render.daylight(false)
  render.atmosphere(false)
  render.palette(false)
  render.clear_color(0.08, 0.06, 0.14)
  render.light({ direction = { -0.4, -1, -0.3 }, color = { 1, 0.92, 0.8 }, ambient = { 0.35, 0.3, 0.4 } })
end

-- Switches between "flight", "create" and "showcase" (scenarios use it too).
function game.set_mode(mode)
  if game.mode == "create" and mode ~= "create" and game.creation then
    creation.cancel(game.creation)
  end
  game.mode = mode
  if mode == "flight" then
    setup_flight()
  elseif mode == "create" then
    setup_create()
  else
    setup_showcase()
  end
end

-- Saves the world being played (its time, the players' edits, the carpet)
-- and remembers it for the next start.
function game.save()
  local c = game.carpet
  local player = c and { pos = c.pos, heading = c.heading, magic = game.magic and magic.snapshot(game.magic) } or nil
  local ok, err = world.save_game(game.save_name, player)
  if ok then
    save.set("world", game.save_name)
    save.flush()
  else
    print("could not save the game " .. game.save_name .. ": " .. tostring(err))
  end
  return ok, err
end

-- Plays in the finished world and remembers it for the next start.
function game.fly_in_new_world()
  local name = "world-" .. tostring(game.creation.progress and engine.info().seed or 0)
  local ok, err = creation.finish(game.creation, name)
  if ok then
    game.save_name = name
    game.creation = nil
    game.set_mode("flight")
    game.save()
  end
  return ok, err
end

local function setup_touch()
  if game.mode == "flight" then
    controls.setup_touch()
  else
    touch.setup()
  end
end

engine.on("init", function()
  assets.carpet = render.model("models/carpet.glb")
  assets.mote = render.sprite("sprites/mote.lua")
  assets.flame = render.sprite("sprites/flame.lua")
  assets.font = render.font("fonts/pixel.lua")
  game.music = audio.play("music/loop.ogg", { loop = true, group = "music", volume = 0.5 })
  game.credits = engine.credits()
  local saved = save.get("world")
  if saved then -- continue the game saved last time
    local player, err = world.load_game(saved)
    if player then
      game.save_name = saved
      game.restore = type(player) == "table" and player or nil
    else
      print("could not load the game " .. saved .. ": " .. tostring(err))
    end
  end
  game.set_mode(game.mode)
  if input.mode() == "touch" then
    setup_touch()
  end
end)

local function handle(ev)
  if ev == "chime" or ev == "spin" then
    audio.play("sounds/chime.wav", { volume = ev == "spin" and 0.4 or 0.8, pitch = ev == "spin" and 1.5 or 1 })
  elseif ev == "fullscreen" then
    game.fullscreen = not game.fullscreen
    engine.set_fullscreen(game.fullscreen)
  elseif ev == "quit" then
    if game.mode == "flight" then
      game.save()
    end
    engine.quit()
  end
end

local function showcase_tick(dt)
  local inp = {}
  for _, name in ipairs(SHOWCASE_ACTIONS) do
    inp[name] = input.state(name)
  end
  for _, ev in ipairs(state.update(game.state, inp, dt)) do
    handle(ev)
  end
end

local function flight_tick(dt)
  game.flight_ticks = (game.flight_ticks or 0) + 1
  if game.flight_ticks % AUTOSAVE_TICKS == 0 then
    game.save()
  end
  local st = controls.read()
  local co = game.coop
  local c = coop.tick(co, game.carpet, controls.flight_input(st), dt, WORLD, net)
  game.carpet = c
  -- Magic: while a gesture is drawn the mouse draws instead of turning the camera.
  local spell = {}
  for _, name in ipairs(magic.ACTIONS) do spell[name] = input.state(name) end
  local here = mana.at(c.pos)
  game.last_cast = magic.tick(game.magic, spell, dt, c, { heading = c.heading, pitch = -0.12 },
    here.loaded and here.stored or nil) or game.last_cast
  camera.update(game.chase, not spell.draw.held and st.look or nil, dt, input.mode() ~= "touch")
  fauna.tick(game.fauna, dt, c.pos, false)
  world.set_focus(1, c.pos, carpet.velocity(c), carpet.forward(c))
  -- The host streams around every heir (one focus each).
  for p = 1, 3 do
    local other = co.role == "host" and co.carpets[p]
    if other then
      world.set_focus(p + 1, other.pos, carpet.velocity(other), carpet.forward(other))
    else
      world.clear_focus(p + 1)
    end
  end
  -- The shared buttons keep their showcase meaning (chime, credits, fullscreen, quit).
  for _, name in ipairs({ "chime", "fullscreen", "quit" }) do
    if st[name] and st[name].pressed then
      handle(name)
    end
  end
  if st.credits and st.credits.pressed then
    game.state.credits = not game.state.credits
  end
  if st.new_world and st.new_world.pressed then
    game.set_mode("create")
  end
end

local function create_tick(dt)
  creation.tick(game.creation, dt)
  if creation.ready(game.creation) and input.state("confirm").pressed then
    game.fly_in_new_world()
  elseif input.state("quit").pressed then
    game.set_mode("flight")
  end
end

engine.on("tick", function(dt)
  if game.mode == "flight" then
    flight_tick(dt)
  elseif game.mode == "create" then
    create_tick(dt)
  else
    showcase_tick(dt)
  end
end)

local function draw_hud(lines)
  if not game.hud then return end
  if game.state.credits then
    lines[#lines + 1] = "Credits:"
    for _, c in ipairs(game.credits or {}) do
      lines[#lines + 1] = hud.credit_line(c)
    end
  end
  for i, line in ipairs(lines) do
    render.draw_text(assets.font, line, 4, 4 + (i - 1) * 9, 1, hud.color(i))
  end
end

local function flight_frame()
  local c = game.carpet
  local now = engine.time()
  local dt = game.last_frame_time and (now - game.last_frame_time) or nil
  game.last_frame_time = now
  local p = world.to_local(c.pos)
  local up = world.up_at(c.pos)
  if game.camera_override then
    render.camera(game.camera_override())
  else
    local eye, target = camera.view(game.chase, p, c.heading, up, dt)
    render.camera({ eye = eye, target = target, up = up, fov = 65, near = 0.5, far = 30000 })
  end
  render.draw_model(assets.carpet, { position = p, rotation = carpet.rotation(c) })
  local sights = magic.sights(game.magic)
  for _, l in ipairs(sights.lights) do render.draw_sprite(assets.mote, world.to_local(l), 2) end
  for _, b in ipairs(sights.bolts) do
    render.draw_sprite(b.color == "water" and assets.mote or assets.flame, world.to_local(b.pos), 1.2)
  end
  for _, other in pairs(game.coop.carpets) do
    if other ~= c then
      render.draw_model(assets.carpet, { position = world.to_local(other.pos), rotation = carpet.rotation(other) })
    end
  end
  local lines = flight_hud.lines(c, world.ground_height(c.pos), input.mode(), engine.info(), world.time())
  for _, line in ipairs(magic.hud_lines(game.magic)) do lines[#lines + 1] = line end
  for _, line in ipairs(fauna.hud_lines(game.fauna)) do lines[#lines + 1] = line end
  draw_hud(lines)
  for _, t in ipairs(sights.trail) do -- the gesture being drawn
    render.draw_text(assets.font, "o", 80 + t[1], 160 + t[2], 1, { 1, 0.9, 0.5, 1 })
  end
end

local function create_frame()
  render.camera({ eye = { 0, 0.4, 3.2 }, target = { 0, 0, 0 }, fov = 45, near = 0.1, far = 10 })
  world.draw_globe(nil, { 0, 0, 0 }, 1, game.creation.spin)
  local lines = creation.lines(game.creation)
  for i, line in ipairs(lines) do
    render.draw_text(assets.font, line, 4, 4 + (i - 1) * 9, 1, hud.color(i))
  end
end

local function showcase_frame()
  local s = game.state
  if game.camera then -- set by scenarios that look at the world instead
    render.camera(game.camera())
  else
    local eye, target = state.camera(s)
    render.camera({ eye = eye, target = target, fov = 60 })
    render.draw_model(assets.carpet, { position = { 0, state.hover(s), 0 }, rotation = { 0, s.spin, state.bank(s) } })
    for _, p in ipairs(state.motes(s)) do
      render.draw_sprite(assets.mote, p, 1)
    end
  end
  if game.hud then
    local info = engine.info()
    for i, line in ipairs(hud.lines(s, input.mode(), info, game.credits)) do
      render.draw_text(assets.font, line, 4, 4 + (i - 1) * 9, 1, hud.color(i))
    end
  end
end

engine.on("frame", function()
  if game.mode == "flight" then
    flight_frame()
  elseif game.mode == "create" then
    create_frame()
  else
    showcase_frame()
  end
end)

-- The simulation state that replays and the determinism checks hash.
engine.on("hash", function()
  return { mode = game.mode, showcase = game.state, carpet = game.carpet and carpet.snapshot(game.carpet),
    magic = game.magic and magic.hash_state(game.magic) }
end)

engine.on("event", function(name, args)
  if name == "input_mode" then
    game.input_mode_event = args.value
    if args.value == "touch" then
      setup_touch()
    end
  elseif name == "resize" then
    game.drawable = { args.width, args.height }
  end
end)

-- Exposed for scenario tests.
_G.demo = {
  game = game, assets = assets, state = state, touch = touch, carpet = carpet, controls = controls, coop = coop,
  creation = creation, magic = magic, fauna = fauna,
}
