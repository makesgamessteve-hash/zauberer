-- SPDX-License-Identifier: MIT
-- Entry script of the base package. Two modes:
--  * "flight" (default, M1 First Flight): a carpet over the streamed planet,
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

local game = { mode = "flight", state = state.new(), fullscreen = false, hud = true }
local assets = {}

local SHOWCASE_ACTIONS = { "turn", "look", "zoom", "spin", "chime", "credits", "fullscreen", "quit" }
local WORLD = { move = world.move, height = world.height, chunk_hash = world.chunk_hash }
local SUN = { 0.45, 0.6, -0.66 }

local function setup_flight()
  local p = world.planet()
  local pos = spawn.find(world.height, p.chunks_per_edge)
  pos.alt = math.max(world.height(pos), 0) + 60
  game.carpet = carpet.new(pos, 0)
  game.chase = camera.new()
  game.coop = coop.new()
  game.last_frame_time = nil
  render.light({ direction = { -SUN[1], -SUN[2], -SUN[3] }, color = { 1, 0.95, 0.85 }, ambient = { 0.38, 0.4, 0.48 } })
  render.atmosphere({ zenith = { 0.2, 0.4, 0.78 }, horizon = { 0.72, 0.8, 0.88 }, sun = SUN,
    sun_color = { 1, 0.95, 0.8 }, fog_distance = 12000 })
  render.palette({})
  world.set_focus(1, game.carpet.pos, { 0, 0, 0 }, carpet.forward(game.carpet))
end

local function setup_showcase()
  world.clear_focus(1)
  render.atmosphere(false)
  render.palette(false)
  render.clear_color(0.08, 0.06, 0.14)
  render.light({ direction = { -0.4, -1, -0.3 }, color = { 1, 0.92, 0.8 }, ambient = { 0.35, 0.3, 0.4 } })
end

-- Switches between "flight" and "showcase" (scenarios use it too).
function game.set_mode(mode)
  game.mode = mode
  if mode == "flight" then
    setup_flight()
  else
    setup_showcase()
  end
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
  assets.font = render.font("fonts/pixel.lua")
  game.music = audio.play("music/loop.ogg", { loop = true, group = "music", volume = 0.5 })
  game.credits = engine.credits()
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
  local st = controls.read()
  local co = game.coop
  local c = coop.tick(co, game.carpet, controls.flight_input(st), dt, WORLD, net)
  game.carpet = c
  camera.update(game.chase, st.look, dt, input.mode() ~= "touch")
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
end

engine.on("tick", function(dt)
  if game.mode == "flight" then
    flight_tick(dt)
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
  for _, other in pairs(game.coop.carpets) do
    if other ~= c then
      render.draw_model(assets.carpet, { position = world.to_local(other.pos), rotation = carpet.rotation(other) })
    end
  end
  draw_hud(flight_hud.lines(c, world.ground_height(c.pos), input.mode(), engine.info()))
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
  else
    showcase_frame()
  end
end)

-- The simulation state that replays and the determinism checks hash.
engine.on("hash", function()
  return { mode = game.mode, showcase = game.state, carpet = game.carpet and carpet.snapshot(game.carpet) }
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
}
