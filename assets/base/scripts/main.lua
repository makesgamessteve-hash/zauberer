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
local ecology = require("life.ecology")
local plants = require("life.plants")
local smalllife = require("life.smalllife")
local motes = require("magic.motes")
local effects = require("magic.effects")
local health = require("player.health")
local walker = require("player.walker")
local survival = require("player.survival")

local game = { mode = "flight", state = state.new(), fullscreen = false, hud = true, save_name = "default" }
local AUTOSAVE_TICKS = 5 * 60 * 60 -- every five minutes of flight
local assets = {}

local SHOWCASE_ACTIONS = { "turn", "look", "zoom", "spin", "chime", "credits", "fullscreen", "quit" }
local WORLD_START = 8 * 3600 -- a new world's time (game seconds)
local WORLD = { move = world.move, height = world.height, water = world.water, chunk_hash = world.chunk_hash }
local FOOT = { move = world.move, height = world.ground_height, water = world.water } -- the ground as drawn
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
  game.home = { pos = world.move(pos, 0, 0, 0), heading = heading } -- where a defeated player gets up again
  local back = game.restore -- the player of a loaded save
  if back and type(back.pos) == "table" then
    pos, heading = back.pos, back.heading or 0
  end
  game.restore = nil
  if world.time().time == WORLD_START then
    world.set_time(morning_at(pos))
  end
  game.carpet = carpet.new(pos, heading)
  game.walker = back and type(back.on_foot) == "table" and back.on_foot.pos
    and walker.new(back.on_foot.pos, back.on_foot.heading, FOOT) or nil
  game.magic = magic.new(back and back.magic)
  game.health = health.new(back and back.health)
  game.survival = survival.new(back and back.survival, save.get("difficulty"))
  game.survival_at = nil
  if game.fauna then
    fauna.clear(game.fauna)
  end
  game.fauna = fauna.new(pos, heading)
  -- The land's populations and what was taken from its plants: from the save,
  -- kept when flying on in the same world, new in a new one.
  local world_id = tostring((world.info() or {}).seed or ("noise " .. engine.info().seed))
  if game.ecology then
    ecology.fold_all(game.ecology)
  end
  if back or game.life_world ~= world_id then
    local life = back and back.life or {}
    game.ecology = ecology.new(life.ecology)
    game.plants = plants.new(life.plants)
    game.smalllife = smalllife.new(life.smalllife)
  end
  game.life_world = world_id
  game.motes = motes.new(engine.info().seed)
  game.channeling = false
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
  local player = c and { pos = c.pos, heading = c.heading, magic = game.magic and magic.snapshot(game.magic),
    health = game.health and health.snapshot(game.health),
    survival = game.survival and survival.snapshot(game.survival),
    on_foot = game.walker and walker.snapshot(game.walker),
    life = game.ecology and { ecology = ecology.snapshot(game.ecology),
      plants = plants.snapshot(game.plants, world.time().time),
      smalllife = smalllife.snapshot(game.smalllife, world.time().time) } } or nil
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
  for _, kind in ipairs({ "bee", "mosquito", "firefly", "butterfly" }) do
    assets[kind] = render.sprite("sprites/" .. kind .. ".lua")
  end
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
  local down = game.health.down ~= nil -- a defeated player lies still
  local still = { turn = 0, throttle = 0, climb = 0, boost = false }
  -- Stepping off a carpet resting low and slow, and back on.
  local w = game.walker
  if not down and input.state("mount").pressed then
    if w and walker.near_carpet(w, game.carpet) then
      game.walker, w = nil, nil
    elseif not w and walker.can_step_off(game.carpet, FOOT) then
      game.carpet.speed = 0
      game.walker = walker.step_off(game.carpet, FOOT)
      w = game.walker
    end
  end
  local c = coop.tick(co, game.carpet, (down or w) and still or controls.flight_input(st), dt, WORLD, net)
  game.carpet = c
  -- Magic: while a gesture is drawn the mouse draws instead of turning the view.
  local spell = {}
  for _, name in ipairs(magic.ACTIONS) do spell[name] = input.state(name) end
  if down then
    for name in pairs(spell) do spell[name] = {} end
  end
  local look = not spell.draw.held and st.look or nil
  if w then
    local steer = st.steer or {}
    local walking = { turn = controls.curve(steer.x or 0), throttle = controls.curve(steer.y or 0),
      run = st.boost and st.boost.held, jump = input.state("jump").pressed, look = look,
      mouse = input.mode() ~= "touch" }
    walker.update(w, down and {} or walking, dt, FOOT)
  else
    camera.update(game.chase, look, dt, input.mode() ~= "touch")
  end
  local me = w and w.pos or c.pos -- where the player is
  local here = mana.at(me)
  local hands = w and walker.hands(w) or c
  local cast_now = magic.tick(game.magic, spell, dt, hands, { heading = hands.heading, pitch = w and w.pitch or -0.12 },
    here.loaded and here.stored or nil)
  game.last_cast = cast_now or game.last_cast
  -- Creatures, and the motes of the magical ones that die.
  local now = world.time().time
  ecology.tick(game.ecology, dt, me, now)
  local creature_events = fauna.tick(game.fauna, dt, me, w ~= nil)
  for _, ev in ipairs(creature_events) do
    if ev.kind == "died" and ev.magical and ev.mana and (ev.motes or 0) > 0 then
      motes.release(game.motes, ev.pos, ev.mana, ev.motes, 8)
    end
    if ev.kind == "died" then smalllife.died(game.smalllife, ev.id, now) end -- carcasses rot away
  end
  smalllife.tick(game.smalllife, dt, me, now) -- mushrooms, microbes, insects
  -- Harm and healing; a defeated player gets up again where the carpet set out.
  local fx = game.magic.caster.effects
  local fate = health.tick(game.health, dt, { pos = me, on_foot = w ~= nil, landing = w and w.landing,
    events = creature_events,
    heard = effects.take_events(fx), cast = cast_now, wards = function(kind) return effects.warded(fx, kind) end })
  if fate == "revived" then
    game.carpet = carpet.new(world.move(game.home.pos, 0, 0, 0), game.home.heading)
    c = game.carpet
    game.walker, w = nil, nil
    me = c.pos
    camera.reset(game.chase)
  end
  -- Food, water, rest and warmth run in game time (a time skip is not counted
  -- here: sleep counts its own).
  local game_dt = math.min(math.max(now - (game.survival_at or now), 0), 300)
  game.survival_at = now
  local body = survival.sense(me, w, smalllife.bites(game.smalllife, me))
  for _, h in ipairs(survival.tick(game.survival, game_dt, body)) do
    health.hurt(game.health, h[1], h[2], function(kind) return effects.warded(fx, kind) end)
  end
  if w and input.state("sleep").pressed and not game.health.down then
    local hours, harms = survival.sleep(game.survival, 10, survival.sense(me, w, 0))
    for _, h in ipairs(harms) do health.hurt(game.health, h[1], h[2]) end
    world.set_time(now + hours * 3600)
    game.survival_at = now + hours * 3600
    game.slept = hours
  end
  local m = game.magic.caster.mage
  motes.tick(game.motes, dt, { { pos = me, mage = m } })
  motes.bloom(game.motes, me, dt)
  game.channeling = input.state("channel").held and motes.channel(m, me, dt, w and math.abs(w.speed) or c.speed) > 0
  if w then
    world.set_focus(1, me, { 0, 0, 0 }, { math.sin(w.heading), 0, -math.cos(w.heading) })
  else
    world.set_focus(1, c.pos, carpet.velocity(c), carpet.forward(c))
  end
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

-- The HUD in flight: the carpet, health, magic, creatures and motes.
local function flight_lines()
  local c = game.carpet
  local lines = flight_hud.lines(c, world.ground_height(c.pos), input.mode(), engine.info(), world.time())
  local parts = { health.hud_lines(game.health), survival.hud_lines(game.survival), magic.hud_lines(game.magic),
    fauna.hud_lines(game.fauna), motes.hud_lines(game.motes) }
  for _, more in ipairs(parts) do
    for _, line in ipairs(more) do lines[#lines + 1] = line end
  end
  if game.channeling then lines[#lines + 1] = "Channeling" end
  local w = game.walker
  if w then
    lines[#lines + 1] = "On foot" .. (w.on_ice and ", on the ice" or "") .. (w.blocked and " - too deep to wade" or "")
  elseif walker.can_step_off(c, FOOT) then
    lines[#lines + 1] = "Low and slow: E to step off"
  end
  return lines
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
  elseif game.walker then -- on foot: out of the eyes
    local eye, target = walker.view(game.walker)
    render.camera({ eye = eye, target = target, up = world.up_at(game.walker.pos), fov = 70, near = 0.1, far = 30000 })
  else
    local eye, target = camera.view(game.chase, p, c.heading, up, dt)
    render.camera({ eye = eye, target = target, up = up, fov = 65, near = 0.5, far = 30000 })
  end
  render.draw_model(assets.carpet, { position = p, rotation = carpet.rotation(c) })
  local sights = magic.sights(game.magic)
  for _, l in ipairs(sights.lights) do render.draw_sprite(assets.mote, world.to_local(l), 2) end
  for _, mote in ipairs(game.motes.list) do render.draw_sprite(assets.mote, world.to_local(mote.pos), 0.7) end
  for _, bug in ipairs(smalllife.sights(game.smalllife)) do render.draw_sprite(assets[bug.kind], bug.pos, 1) end
  for _, b in ipairs(sights.bolts) do
    render.draw_sprite(b.color == "water" and assets.mote or assets.flame, world.to_local(b.pos), 1.2)
  end
  for _, other in pairs(game.coop.carpets) do
    if other ~= c then
      render.draw_model(assets.carpet, { position = world.to_local(other.pos), rotation = carpet.rotation(other) })
    end
  end
  draw_hud(flight_lines())
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
  creation = creation, magic = magic, fauna = fauna, motes = motes, health = health, flight_lines = flight_lines,
  walker = walker, ecology = ecology, plants = plants, smalllife = smalllife, survival = survival,
}
