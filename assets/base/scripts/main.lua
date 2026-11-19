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
local pack = require("player.pack")
local bag = require("player.bag")
local condition = require("flight.condition")
local cloak = require("flight.cloak")
local tent = require("flight.tent")
local home = require("player.home")
local alchemy = require("magic.alchemy")
local crafting = require("magic.crafting")
local artificing = require("magic.artificing")
local ritual = require("magic.ritual")
local summoning = require("magic.summoning")
local disciplines = require("magic.disciplines")
local harvest = require("life.harvest")
local cooking = require("life.cooking")
local items = require("life.items")
local codex = require("life.codex")
local placeplay = require("places.play")
local director = require("story.director")
local prologue = require("story.prologue")
local coopplay = require("coop.play")
local saga = require("story.saga")
local weave = require("coop.weave")
local text = require("ui.text")
local settings = require("ui.settings")
local access = require("ui.access")
local menu = require("ui.menu")
local modapi = require("mods.api")

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
  game.pack = pack.new(back and back.pack)
  game.bag = bag.new(back and back.bag)
  game.condition = condition.new(back and back.condition)
  game.cloak = cloak.new(back and back.cloak)
  game.tent = tent.new(back and back.tent)
  game.home_place = home.new(back and back.home)
  game.artifacts = artificing.restore(back and back.artifacts)
  game.ritual = ritual.new(back and back.ritual)
  if game.summoning then summoning.dismiss(game.summoning) end
  game.summoning = summoning.new(back and back.summoning)
  game.disciplines = disciplines.new(back and back.disciplines)
  game.known = {} -- the substances analysed
  for _, id in ipairs(back and type(back.known) == "table" and back.known or {}) do game.known[id] = true end
  game.codex = codex.new(back and back.codex)
  -- The peopled world (made in slices as play begins), Anarvan where the carpet set out.
  game.world_places = placeplay.setup(back and back.world_places, (world.info() or {}).seed or engine.info().seed,
    game.home.pos, FOOT, game.world_options and game.world_options.history)
  game.story, game.story_saved = nil, back and back.story -- (the prologue resumes once the world is peopled)
  game.saga, game.saga_saved = nil, back and back.saga -- (and the acts after it)
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
  game.together = coopplay.new(back and back.together, (game.world_options or {}).heirs or 4)
  game.last_frame_time = nil
  render.light({ direction = { -SUN[1], -SUN[2], -SUN[3] }, color = { 1, 0.95, 0.85 }, ambient = { 0.38, 0.4, 0.48 } })
  render.atmosphere({ zenith = { 0.2, 0.4, 0.78 }, horizon = { 0.72, 0.8, 0.88 }, sun = SUN,
    sun_color = { 1, 0.95, 0.8 }, fog_distance = 12000 })
  render.palette(access.palette())
  render.daylight(true) -- the sun, the moon and the seasons over the carpet
  world.set_focus(1, game.carpet.pos, { 0, 0, 0 }, carpet.forward(game.carpet))
end

local function setup_create()
  world.clear_focus(1)
  render.daylight(false)
  render.atmosphere(false)
  render.palette(access.palette())
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
    survival = game.survival and survival.snapshot(game.survival), pack = game.pack and pack.snapshot(game.pack),
    bag = game.bag and bag.snapshot(game.bag), condition = game.condition and condition.snapshot(game.condition),
    cloak = game.cloak and cloak.snapshot(game.cloak), tent = tent.snapshot(game.tent),
    artifacts = game.artifacts and artificing.snapshot(game.artifacts),
    ritual = game.ritual and ritual.snapshot(game.ritual),
    summoning = game.summoning and summoning.snapshot(game.summoning),
    disciplines = game.disciplines and disciplines.snapshot(game.disciplines),
    home = game.home_place and home.snapshot(game.home_place),
    world_places = game.world_places and placeplay.snapshot(game.world_places),
    story = game.story and prologue.snapshot(game.story), saga = game.saga and saga.snapshot(game.saga),
    together = game.together and coopplay.snapshot(game.together), known = game.known and (function()
      local list = {}
      for id in pairs(game.known) do list[#list + 1] = id end
      table.sort(list)
      return list
    end)(),
    codex = game.codex and codex.snapshot(game.codex),
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
  assets.tent = render.model("models/tent.glb")
  assets.room = render.model("models/room.glb")
  assets.mote = render.sprite("sprites/mote.lua")
  assets.flame = render.sprite("sprites/flame.lua")
  assets.person = render.sprite("sprites/person.lua")
  for _, kind in ipairs({ "bee", "mosquito", "firefly", "butterfly" }) do
    assets[kind] = render.sprite("sprites/" .. kind .. ".lua")
  end
  assets.font = render.font("fonts/pixel.lua")
  game.setup_ui()
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
    game.quit_to_desktop()
  end
end

-- The player's settings (language, accessibility), the input they shape and
-- the menu (again after a script reload in development).
function game.setup_ui()
  if game.menu then return end
  game.settings = settings.load(engine.info().language)
  if not input.raw_state then access.wrap_input(input, menu.ACTIONS) end
  game.menu = menu.new(game.settings, {
    quit = function() game.quit_to_desktop() end,
    resume = function() access.block(false) end,
    packages = engine.packages,
    set_package = engine.set_package_enabled,
  })
  game.captions = access.captions_new()
  settings.listeners[#settings.listeners + 1] = function(s, name)
    if name == "survival" and game.survival then game.survival.preset = s.survival end
    if name == "colour_vision" and game.mode ~= "showcase" then render.palette(access.palette()) end
  end
end

-- Sound captions: lightning, fire close by and calling creatures, with the side they are on.
local function bearing(from, to)
  local a, b = world.to_local(from), world.to_local(to)
  return math.atan(b[1] - a[1], a[3] - b[3]) % (2 * math.pi)
end

function game.listen()
  local now = world.time().time
  if not access.option("captions") or (game.captions.next_listen or 0) > now or not game.carpet then return end
  local me = (game.walker and game.walker.pos) or game.carpet.pos
  local heading = (game.walker and game.walker.heading) or game.carpet.heading or 0
  local fires, creatures = {}, {}
  for k = 0, 7 do
    local b = k * math.pi / 4
    if sim.fire_at(world.move(me, b, 20)).burning then fires[#fires + 1] = b end
  end
  for _, id in ipairs(sim.creatures_near(me, 80)) do
    local cr = sim.creature(id)
    if cr then creatures[#creatures + 1] = { species = cr.species, alive = cr.alive, bearing = bearing(me, cr.pos) } end
  end
  access.listen(game.captions, now, { heading = heading, strikes = sim.weather().strikes, fires = fires,
    creatures = creatures })
end

-- Saves (in flight) and ends the game.
function game.quit_to_desktop()
  if game.mode == "flight" then
    game.save()
  end
  engine.quit()
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

-- Inside the tent: the pocket home. The world outside goes on around the
-- tent (the mage is not in it); the body, the pack and the bag go on inside
-- in the warm.
local function home_tick(dt, st)
  local t, h = game.tent, game.home_place
  local now = world.time().time
  home.tick(h, dt)
  disciplines.tick(game.disciplines, dt)
  artificing.tick(game.artifacts, dt, { mage = game.magic.caster.mage, pos = t.pos })
  game.codex.focus = nil -- (nothing of the world is in sight)
  ecology.tick(game.ecology, dt, t.pos, now)
  fauna.tick(game.fauna, dt, t.pos, false, { visibility = 0, presence = 0 })
  smalllife.tick(game.smalllife, dt, t.pos, now)
  local game_dt = math.min(math.max(now - (game.survival_at or now), 0), 300)
  game.survival_at = now
  local body = home.body(h)
  for _, harm in ipairs(survival.tick(game.survival, game_dt, body)) do health.hurt(game.health, harm[1], harm[2]) end
  survival.tick_news(game.survival, dt)
  health.heal(game.health, health.REGEN * dt)
  pack.spoil(game.pack, game_dt / 3600, body.air, false)
  pack.tick(game.pack, dt)
  bag.tick(game.bag, dt)
  bag.spoil(game.bag, game_dt / 3600, body.air)
  -- The tent outside: rain falls on it and flames can reach it.
  local cell = sim.fire_at(t.pos)
  condition.tick(game.condition, dt, game_dt / 3600, { exposed = true, height = 0,
    burning = cell.loaded and cell.burning,
    rain = sim.weather().rain, air = sim.weather().temperature })
  -- Walking between the rooms; going out.
  local x = (st.steer and st.steer.x) or 0
  if math.abs(x) > 0.5 and not h.latched then home.walk(h, x > 0 and 1 or -1) end
  h.latched = math.abs(x) > 0.5
  local function said(list)
    local parts = {}
    for id, n in pairs(list) do parts[#parts + 1] = items.name(id):lower() .. " " .. n end
    table.sort(parts)
    return table.concat(parts, ", ")
  end
  if input.state("mount").pressed then
    t.inside = false
    game.walker.pos = tent.door(t)
    return
  end
  -- Hours at the bench are lived (as sleep counts its own).
  local function spend(hours)
    for _, harm in ipairs(survival.tick(game.survival, hours * 3600, body)) do
      health.hurt(game.health, harm[1], harm[2])
    end
    world.set_time(now + hours * 3600)
    game.survival_at = now + hours * 3600
  end
  if home.offers(h, "craft") then
    local works = {}
    for _, cr in ipairs(crafting.crafts()) do works[#works + 1] = { craft = cr } end
    for _, e in ipairs(artificing.enchantments()) do works[#works + 1] = { enchant = e } end
    h.work = math.min(h.work or 1, #works)
    if input.state("cloak_colour").pressed then h.work = h.work % #works + 1 end
    local job = works[h.work]
    if input.state("cook").pressed and job.craft then
      local made, hours = crafting.craft(job.craft, game.pack, game.condition, condition)
      if made then
        spend(hours)
        home.say(h, next(made) and ("Made " .. said(made)) or (job.craft.name .. ": done"))
        game.crafted = job.craft.id
        disciplines.practice(game.disciplines, "artificing", 5, "craft", job.craft.id)
      else
        home.say(h, hours)
      end
    elseif input.state("cook").pressed then
      local made, why = artificing.enchant(job.enchant, { pack = game.pack, mage = game.magic.caster.mage,
        condition = game.condition, cmod = condition, bag = game.bag, artifacts = game.artifacts,
        factor = disciplines.factor(game.disciplines, "artificing", true) })
      home.say(h, made and ("Enchanted: " .. job.enchant.name) or why)
      if made then
        game.enchanted = job.enchant.id
        disciplines.practice(game.disciplines, "artificing", 25, "enchant", job.enchant.id)
      end
    end
  end
  if home.offers(h, "read") then -- the study: Farrokh's books
    h.study = math.min(h.study or 1, #disciplines.LIST)
    if input.state("cloak_colour").pressed then h.study = h.study % #disciplines.LIST + 1 end
    if input.state("cook").pressed then
      spend(2)
      disciplines.study(game.disciplines, disciplines.LIST[h.study], 2)
      home.say(h, "Two hours with Farrokh's books on " .. disciplines.LIST[h.study])
    end
  end
  if home.offers(h, "brew") then
    local list = alchemy.recipes()
    h.recipe = math.min(h.recipe or 1, #list)
    if input.state("cloak_colour").pressed then h.recipe = h.recipe % #list + 1 end
    if input.state("cook").pressed then
      local made, hours = alchemy.brew(list[h.recipe], game.pack, game.magic.caster.mage, game.known,
        disciplines.factor(game.disciplines, "alchemy"))
      if made then
        spend(hours)
        home.say(h, next(made) and ("Brewed " .. said(made)) or "Nothing came of it")
        game.brewed = made
        disciplines.practice(game.disciplines, "alchemy", 15, "brew", list[h.recipe].id)
      else
        home.say(h, hours)
      end
    end
  elseif input.state("cook").pressed and not home.offers(h, "craft") and not home.offers(h, "read") then
    local made, why = cooking.cook(game.pack, t.pos, home.offers(h, "cook"))
    home.say(h, made and ("Cooked " .. said(made)) or (home.offers(h, "cook") and why or "no fire in this room"))
  end
  for action, fn in pairs({ eat = cooking.eat, drink = cooking.drink }) do
    if input.state(action).pressed then
      local name, why = fn(game.pack, game.survival, function(item) return codex.warned(game.codex, item) end)
      home.say(h, name and ("You " .. action .. " " .. name:lower()) or why)
    end
  end
  if game.survival.heal then
    health.heal(game.health, game.survival.heal)
    game.survival.heal = nil
  end
  if input.state("sleep").pressed then
    if home.offers(h, "sleep") then
      local hours, harms = survival.sleep(game.survival, 10, body)
      for _, harm in ipairs(harms) do health.hurt(game.health, harm[1], harm[2]) end
      health.heal(game.health, hours * 10) -- (rest mends)
      world.set_time(now + hours * 3600)
      game.survival_at = now + hours * 3600
      game.slept = hours
    else
      home.say(h, "The bed is in the bedchamber")
    end
  end
  if home.offers(h, "vault") then
    if input.state("stow").pressed then
      local moved = bag.shelve_all(game.bag, game.pack)
      home.say(h, next(moved) and ("Shelved " .. said(moved)) or "Nothing to shelve")
    end
    if input.state("unstow").pressed then
      local moved = bag.take_from_shelves(game.bag, game.pack)
      home.say(h, next(moved) and ("Taken off the shelves " .. said(moved)) or "Nothing taken")
    end
  else
    if input.state("stow").pressed then
      local moved = bag.stow(game.bag, game.pack)
      bag.say(game.bag, next(moved) and ("Stowed " .. said(moved)) or "Nothing to stow")
    end
    if input.state("unstow").pressed then
      local moved = bag.unstow(game.bag, game.pack, game.magic.caster.mage)
      bag.say(game.bag, next(moved) and ("Taken out " .. said(moved)) or "Nothing taken out")
    end
  end
  if input.state("codex").pressed then codex.browse(game.codex) end
  world.set_focus(1, t.pos, { 0, 0, 0 }, { 0, 0, -1 })
end

local function flight_tick(dt)
  game.flight_ticks = (game.flight_ticks or 0) + 1
  if game.flight_ticks % AUTOSAVE_TICKS == 0 then
    game.save()
  end
  game.setup_ui()
  local st = controls.read()
  -- Escape opens the menu; while it is open the heir's own actions are idle (the world goes on).
  if game.menu.open then
    menu.tick(game.menu, input.state, dt)
    access.block(game.menu.open)
  elseif st.quit and st.quit.pressed then
    menu.toggle(game.menu)
    access.block(true)
    st = controls.read()
  end
  game.listen()
  modapi.tick(game.mods, dt)
  if game.tent and game.tent.inside and game.walker then
    home_tick(dt, st)
    return
  end
  local co = game.coop
  local down = game.health.down ~= nil -- a defeated player lies still
  local still = { turn = 0, throttle = 0, climb = 0, boost = false }
  -- Stepping off a carpet resting low and slow, and back on.
  local w = game.walker
  -- The cloak: wrapped from the carpet beside the mage, or from the air (they
  -- glide down in it); spread out again on the ground beside them.
  local k = game.cloak
  if k.worn and not w then k.worn = false end -- (a save from the air)
  if not down and input.state("cloak").pressed then
    if k.worn then
      k.worn = false
      local spot = world.move(w.pos, w.heading, 1.5, 0)
      spot.alt = world.ground_height(spot) + carpet.CLEARANCE
      game.carpet.pos, game.carpet.speed, game.carpet.heading = spot, 0, w.heading
    elseif w and walker.near_carpet(w, game.carpet) then
      k.worn = true
    elseif not w then
      k.worn = true
      local c0 = game.carpet
      game.walker = walker.new(world.move(c0.pos, 0, 0, 0), c0.heading, FOOT)
      w = game.walker
      w.pos = world.move(c0.pos, 0, 0, 0) -- (in the air where the carpet was)
      w.grounded, w.speed = w.pos.alt <= walker.floor(w.pos, FOOT) + 0.05, math.min(c0.speed, walker.RUN)
      c0.speed = 0
    end
  end
  if input.state("cloak_colour").pressed and k.worn then cloak.next_colour(k) end
  if w then w.glide = k.worn end
  if k.worn then -- (the carpet goes where the mage goes)
    game.carpet.pos, game.carpet.speed = world.move(w.pos, 0, 0, 0), 0
  end
  -- The tent: pitched from the carpet beside the mage, entered at its door,
  -- packed up (the carpet laid out again).
  local t = game.tent
  if not down and w and input.state("tent").pressed then
    if t and tent.near(t, w.pos) then
      game.tent, t = nil, nil
      game.carpet.pos, game.carpet.speed = world.move(game.carpet.pos, 0, 0, 0), 0
      game.carpet.pos.alt = world.ground_height(game.carpet.pos) + carpet.CLEARANCE
    elseif not t and not k.worn and walker.near_carpet(w, game.carpet) and co.role ~= "none"
      and not weave.can_pitch(game.together.weave) then
      condition.say(game.condition, select(2, weave.can_pitch(game.together.weave)))
    elseif not t and not k.worn and walker.near_carpet(w, game.carpet) then
      local pitched, why = tent.pitch(w.pos, w.heading)
      if pitched then
        game.tent, t = pitched, pitched
        game.carpet.pos, game.carpet.speed = world.move(pitched.pos, 0, 0, 0), 0
      else
        condition.say(game.condition, why)
      end
    end
  end
  if t and not t.inside then game.carpet.pos, game.carpet.speed = world.move(t.pos, 0, 0, 0), 0 end
  if not down and w and t and input.state("mount").pressed and tent.near(t, w.pos) then
    t.inside = true
    game.home_place.room = "hearth"
    return
  end
  if not down and not k.worn and not t and input.state("mount").pressed then
    if w and walker.near_carpet(w, game.carpet) then
      game.walker, w = nil, nil
    elseif not w and walker.can_step_off(game.carpet, FOOT) then
      game.carpet.speed = 0
      game.walker = walker.step_off(game.carpet, FOOT)
      w = game.walker
    end
  end
  game.carpet.limits = condition.limits(game.condition) -- (wet, torn or grounded, it flies worse)
  if game.story and not prologue.can_fly(game.story) then -- (the carpet has not chosen the heir yet)
    local lim = {}
    for key, v in pairs(game.carpet.limits or {}) do lim[key] = v end
    lim.fly = false
    game.carpet.limits = lim
  end
  local c = coop.tick(co, game.carpet, (down or w) and still or controls.flight_input(st), dt, WORLD, net)
  game.carpet = c
  -- Co-op: the heirs, the Weave, the host's clock, votes and the active areas.
  if co.role ~= "none" then
    local T = game.together
    coopplay.tick(T, co, { now = world.time().time, netapi = net, set_time = world.set_time,
      sleep = function(hours) world.set_time(world.time().time + hours * 3600) end,
      in_home = { [1] = game.tent ~= nil and game.tent.inside or nil },
      past_prologue = game.story ~= nil and game.story.beat ~= "village",
      world_info = function()
        return { seed = (world.info() or {}).seed, name = game.save_name, bundle = world.export_bundle(),
          history = nil }
      end,
      areas = { set_focus = world.set_focus, clear_focus = world.clear_focus, set_radius = sim.set_active_radius,
        distance = world.distance }, tier = sim.tier_stats })
    for _, line in ipairs(coopplay.take_said(T)) do pack.say(game.pack, line) end
    if input.state("weave").pressed then
      coopplay.ask(T, co, net, T.weave.whole and "split" or "rejoin", { now = world.time().time })
    end
  end
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
  if cast_now and cast_now.ok then
    disciplines.practice(game.disciplines, "spellcasting", 2, "cast", cast_now.name or "free weave")
  end
  -- Creatures, and the motes of the magical ones that die.
  local now = world.time().time
  local hp_before = game.health.hp
  ecology.tick(game.ecology, dt, me, now)
  local visibility, presence = cloak.seen(k, { flying = w == nil and c.pos.alt - world.ground_height(c.pos) > 3,
    speed = w and math.abs(w.speed) or c.speed, silent = condition.has(game.condition, "silent") })
  local creature_events = fauna.tick(game.fauna, dt, me, w ~= nil, { visibility = visibility, presence = presence })
  for _, ev in ipairs(creature_events) do
    if ev.kind == "died" and ev.magical and ev.mana and (ev.motes or 0) > 0 then
      motes.release(game.motes, ev.pos, ev.mana, ev.motes, 8)
    end
    if ev.kind == "died" then smalllife.died(game.smalllife, ev.id, now) end -- carcasses rot away
  end
  smalllife.tick(game.smalllife, dt, me, now) -- mushrooms, microbes, insects
  artificing.tick(game.artifacts, dt, { mage = game.magic.caster.mage, pos = me }) -- (artifacts at work)
  -- The Codex: what is in sight is seen, what is looked at is observed.
  local view
  if w then
    local cp = math.cos(w.pitch)
    view = { eye = world.move(w.pos, 0, 0, 0, walker.EYE), feet = w.pos,
      dir = { math.sin(w.heading) * cp, math.sin(w.pitch), -math.cos(w.heading) * cp } }
  else
    view = { eye = c.pos, dir = camera.direction(game.chase, c.heading) }
  end
  codex.look(game.codex, dt, view, { smalllife = game.smalllife }, now)
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
  body.cloak = cloak.warmth(k, game.condition.wetness)
  for _, h in ipairs(survival.tick(game.survival, game_dt, body)) do
    health.hurt(game.health, h[1], h[2], function(kind) return effects.warded(fx, kind) end)
  end
  pack.spoil(game.pack, game_dt / 3600, body.air, false)
  pack.tick(game.pack, dt)
  bag.tick(game.bag, dt)
  bag.spoil(game.bag, game_dt / 3600, body.air)
  -- The carpet's condition: burns, tears, rain and drying.
  do
    local ground = world.ground_height(c.pos)
    local cell = sim.fire_at(c.pos)
    local water = sim.water_at(c.pos)
    condition.tick(game.condition, dt, game_dt / 3600, { exposed = not k.worn, height = c.pos.alt - ground,
      speed = c.speed,
      grounded = c.grounded and c.pos.alt - ground < 3, burning = cell.loaded and cell.burning,
      on_water = w ~= nil and water.loaded and water.depth > 0.1 and c.pos.alt - ground < 3,
      rain = sim.weather().rain, air = body.air, fire = cooking.fire_near(c.pos) })
  end
  if w and input.state("patch").pressed and walker.near_carpet(w, c) then
    local mage = game.magic.caster.mage
    local done, why = condition.patch(game.condition, game.pack, mage)
    if not done then condition.say(game.condition, why) end
  end
  survival.tick_news(game.survival, dt)
  -- Gathering, fire and food (on foot), eating and drinking.
  local function said(list)
    local parts = {}
    for id, n in pairs(list) do parts[#parts + 1] = items.name(id):lower() .. " " .. n end
    table.sort(parts)
    return table.concat(parts, ", ")
  end
  if w and input.state("gather").pressed then
    local got, why = harvest.gather(game.pack, { plants = game.plants, smalllife = game.smalllife,
      method = function(id) return codex.method(game.codex, id) end }, w.pos, now, w.heading)
    pack.say(game.pack, got and ("Gathered " .. said(got.items)) or why)
    game.gathered_now = got ~= nil or game.gathered_now
    if got then codex.sampled(game.codex, got.sources) end
  end
  if w and input.state("campfire").pressed then
    local kg, why = cooking.campfire(game.pack, world.move(w.pos, w.heading, 0.6, 0))
    pack.say(game.pack, kg and string.format("A campfire of %.1f kg laid: light it", kg) or why)
  end
  if w and input.state("cook").pressed then
    local made, why = cooking.cook(game.pack, w.pos)
    pack.say(game.pack, made and ("Cooked " .. said(made)) or why)
  end
  for action, fn in pairs({ eat = cooking.eat, drink = cooking.drink }) do
    if input.state(action).pressed then
      local name, why = fn(game.pack, game.survival, function(item) return codex.warned(game.codex, item) end)
      pack.say(game.pack, name and ("You " .. action .. " " .. name:lower()) or why)
    end
  end
  if game.survival.heal then -- (a draught mends)
    health.heal(game.health, game.survival.heal)
    game.survival.heal = nil
  end
  if input.state("codex").pressed then codex.browse(game.codex) end
  -- The bag: the pack's goods into its folds, and the road's needs back out.
  if input.state("stow").pressed then
    local moved = bag.stow(game.bag, game.pack)
    bag.say(game.bag, next(moved) and ("Stowed " .. said(moved)) or "Nothing to stow")
  end
  if input.state("unstow").pressed then
    local moved = bag.unstow(game.bag, game.pack, game.magic.caster.mage)
    bag.say(game.bag, next(moved) and ("Taken out " .. said(moved)) or "Nothing taken out")
  end
  if w and input.state("sleep").pressed and co.role ~= "none" then -- with company: a vote
    local T = game.together
    if T.open_vote then
      coopplay.ask(T, co, net, "ballot", { yes = true })
    else
      coopplay.ask(T, co, net, "vote", { kind = "sleep", now = now })
    end
  elseif w and input.state("sleep").pressed and not game.health.down then
    local hours, harms = survival.sleep(game.survival, 10, survival.sense(me, w, 0))
    for _, h in ipairs(harms) do health.hurt(game.health, h[1], h[2]) end
    world.set_time(now + hours * 3600)
    game.survival_at = now + hours * 3600
    game.slept = hours
  end
  -- Ritual magic: the circle laid and channelled; what rituals done still do.
  local rs = game.ritual
  if w and input.state("ritual_next").pressed then
    rs.chosen = rs.chosen % #ritual.rituals() + 1
    pack.say(game.pack, ritual.line(rs, game.pack))
  end
  if w and input.state("ritual").pressed then
    local ok, why = ritual.begin(rs, ritual.rituals()[rs.chosen], { pos = w.pos, pack = game.pack, on_foot = true,
      daylight = world.sky(me).daylight, near_tent = game.tent ~= nil and world.distance(game.tent.pos, w.pos) < 15,
      factor = disciplines.factor(game.disciplines, "ritual", true) })
    pack.say(game.pack, ok and ("The circle is laid: " .. ritual.rituals()[rs.chosen].name) or why)
  end
  local rr = ritual.tick(rs, dt, { pos = me, mage = game.magic.caster.mage, hurt = game.health.hp < hp_before - 0.5,
    now = now })
  if rr and rr.done then
    pack.say(game.pack, rr.done.name .. ": done" .. (rr.blighted and " - the land is blighted" or ""))
    if rr.done.effect == "room" then
      for _, room in ipairs(home.rooms()) do
        if not game.home_place.open[room.id] then
          home.open_room(game.home_place, room.id)
          pack.say(game.pack, "A new room unfolds in the tent: " .. room.name)
          break
        end
      end
    end
    game.ritual_done = rr.done.id
    if rr.done.effect == "divine" and game.saga then saga.divine(game.saga, now, game.disciplines.tier.ritual) end
    disciplines.practice(game.disciplines, "ritual", 30, "ritual", rr.done.id)
  elseif rr and rr.broken then
    health.hurt(game.health, "magic", rr.backlash)
    pack.say(game.pack, rr.broken.name .. " broken (" .. rr.why .. "): it lashes back")
  end
  local eff = ritual.effects(rs, now)
  if eff.rain then sim.set_weather({ rain = eff.rain, cloud = 1, storm = 0 }) end
  if eff.rain_ended then sim.natural_weather() end
  if eff.ward and math.floor(now) ~= math.floor(now - dt * 36) then sim.stimulus("threat", eff.ward.pos, 1, 2) end
  -- Summoning: call (,), bond a familiar (;), dismiss (-); upkeep.
  local sm = game.summoning
  if input.state("summon").pressed then
    local list = summoning.data().summons
    local done, why
    for _, entry in ipairs(list) do
      done, why = summoning.call(sm, entry, me, game.magic.caster.mage,
        disciplines.factor(game.disciplines, "summoning"))
      if done then
        pack.say(game.pack, "Summoned: " .. entry.name)
        disciplines.practice(game.disciplines, "summoning", 15, "summon", entry.id)
        break
      end
    end
    if not done then pack.say(game.pack, why) end
  end
  if w and input.state("bond").pressed then
    local f, why = summoning.bond(sm, w.pos, game.pack, game.magic.caster.mage, now)
    if f then
      ecology.release(game.ecology, f.creature)
      pack.say(game.pack, "A " .. f.species .. " is your familiar")
      disciplines.practice(game.disciplines, "summoning", 30, "bond", f.species)
    else
      pack.say(game.pack, why)
    end
  end
  if input.state("dismiss").pressed then pack.say(game.pack, "Dismissed " .. summoning.dismiss(sm)) end
  for _, name in ipairs(summoning.tick(sm, dt, game.magic.caster.mage)) do pack.say(game.pack, name .. " fades") end
  summoning.restore_familiar(sm, me)
  if input.state("disciplines").pressed then game.disciplines.shown = not game.disciplines.shown end
  disciplines.tick(game.disciplines, dt)
  do -- naturalism: what the Codex learns
    local n = codex.counts(game.codex)
    local learnt = n[1] + n[2] + n[3] + n[4]
    if game.codex_learnt and learnt > game.codex_learnt then
      for _ = 1, learnt - game.codex_learnt do disciplines.practice(game.disciplines, "naturalism", 5, "learn") end
    end
    game.codex_learnt = learnt
  end
  local m = game.magic.caster.mage
  motes.tick(game.motes, dt, { { pos = me, mage = m } })
  motes.bloom(game.motes, me, dt)
  game.channeling = input.state("channel").held and motes.channel(m, me, dt, w and math.abs(w.speed) or c.speed) > 0
  do -- places and people near every heir; doors and containers used on foot
    local viewers = { me }
    for p = 1, 3 do
      local other = co.role == "host" and co.carpets[p]
      if other then viewers[#viewers + 1] = other.pos end
    end
    placeplay.tick(game.world_places, viewers, function(at, v) -- what the heirs bring to a dungeon's puzzle
      local cell, water = sim.fire_at(at), sim.water_at(at)
      local lit = artificing.working(game.artifacts, "lantern_of_light")
      for _, l in ipairs(magic.sights(game.magic).lights) do lit = lit or world.distance(l, at) < 6 end
      return { heat = cell.loaded and cell.temperature or 0, burnt = cell.loaded and (cell.burnt or cell.burning),
        water = water.loaded and water.depth or 0, light = lit and 1 or 0,
        careful = w ~= nil and math.abs(w.speed) < walker.WALK * 0.7 and world.distance(v, at) < 3, dark = k.worn }
    end, dt)
    if game.world_places.said then
      pack.say(game.pack, game.world_places.said)
      game.world_places.said = nil
    end
    local used_text
    if w and input.state("use").pressed and game.world_places.places.ready then
      local heir = { pack = game.pack, serve = st.boost and st.boost.held, cloak = k.worn and "drab" or "vivid",
        rumour = game.saga and require("story.narrative").rumour(game.saga.N, me),
        magic = game.last_cast ~= nil,
        rest = function(hours) -- a night's lodging
          local h, harms = survival.sleep(game.survival, hours, survival.sense(me, w, 0))
          for _, hm in ipairs(harms) do health.hurt(game.health, hm[1], hm[2]) end
          world.set_time(now + h * 3600)
          game.survival_at = now + h * 3600
          game.slept = h
        end,
        heal = function(n) health.heal(game.health, n) end,
        learn = function(d) disciplines.practice(game.disciplines, d, 20, "taught") end }
      used_text = placeplay.use(game.world_places, w, game.pack, heir)
      if used_text then pack.say(game.pack, used_text) end
    end
    -- The story: begun with F5 in Anarvan (or resumed from the save), fed what the heir does.
    local P = game.world_places.places
    if P.ready and not game.story and (input.state("story").pressed or game.story_saved) then
      game.story = director.start(P)
      local saved = game.story_saved
      game.story_saved = nil
      if type(saved) == "table" and saved.beat then
        for key, v in pairs(saved) do game.story[key] = v end
      else
        game.walker = walker.new(game.story.start, 0, FOOT)
        w = game.walker
        world.set_time(math.floor(now / 86400) * 86400 + 7 * 3600)
      end
    end
    if game.story then
      local ground = world.ground_height(me)
      local before = game.story_boiled or pack.count(game.pack, "boiled_water")
      local happened = director.tick(game.story, P, { pos = me, used = input.state("use").pressed, slept = game.slept,
        dt = dt, now = now, set_time = world.set_time, talked_to = used_text and used_text:match("^(%a+): "),
        found = used_text and used_text:match("^Found (.+)$"), flying = w == nil and me.alt - ground > 3,
        book = game.magic.caster.book, pack = game.pack, gathered = game.gathered_now,
        boiled = pack.count(game.pack, "boiled_water") > before })
      game.story_boiled = pack.count(game.pack, "boiled_water")
      if happened then pack.say(game.pack, game.story.lines[#game.story.lines]) end
      game.slept = nil
    end
    -- Acts II and III: after the prologue (or from the save).
    if P.ready and not game.saga and (game.saga_saved or (game.story and game.story.beat == "done")) then
      game.saga = saga.begin(saga.new(game.saga_saved), P, now)
      game.saga_saved = nil
    end
    if game.saga then
      local void = false
      for _, e in pairs(game.magic.caster.book.journal) do
        for _, col in ipairs(e.weave and e.weave.colors or {}) do void = void or col == "void" end
      end
      local defeated = game.saga.N.defeated and not game.saga.ending
      local done = saga.tick(game.saga, { P = P, pos = me, now = now, dt = dt, economy = game.world_places.folk.economy,
        heirs_power = 40 + 20 * game.disciplines.tier.spellcasting, ritual = game.disciplines.tier.ritual, void = void,
        ash_ward = artificing.working(game.artifacts, "ash_ward"), ally = next(game.summoning.bound) ~= nil,
        calm = ritual.effects(game.ritual, now).calm ~= nil,
        cast = cast_now and cast_now.ok and cast_now.plan and { colours = cast_now.plan.weave.colors,
          power = cast_now.plan.power } or nil,
        hurt = function(harm, colour) health.hurt(game.health, colour == "fire" and "heat" or "magic", harm) end,
        learn = function(d) disciplines.practice(game.disciplines, d, 40, "taught by the Circle") end,
        choose = defeated and input.state("story").pressed, take = defeated and input.state("use").pressed })
      for _, id in ipairs(done) do pack.say(game.pack, "Story: " .. id) end
    end
  end
  if w then
    world.set_focus(1, me, { 0, 0, 0 }, { math.sin(w.heading), 0, -math.cos(w.heading) })
  else
    world.set_focus(1, c.pos, carpet.velocity(c), carpet.forward(c))
  end
  -- (Hosting, coop.play gives every group of heirs its own active area.)
  if co.role == "none" then
    for p = 2, 4 do world.clear_focus(p) end
  end
  -- The shared buttons keep their showcase meaning (chime, credits, fullscreen); Escape is the menu's.
  for _, name in ipairs({ "chime", "fullscreen" }) do
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
  if game.menu and game.menu.open then
    local credits = {}
    for _, c in ipairs(game.credits or {}) do credits[#credits + 1] = { raw = hud.credit_line(c) } end
    text.lines(assets.font, menu.lines(game.menu, credits), hud.color)
    return
  end
  for _, line in ipairs(access.caption_lines(game.captions, world.time().time)) do lines[#lines + 1] = line end
  if game.state.credits then
    lines[#lines + 1] = "Credits:"
    for _, c in ipairs(game.credits or {}) do
      lines[#lines + 1] = { raw = hud.credit_line(c) }
    end
  end
  text.lines(assets.font, lines, hud.color)
end

-- The HUD in flight: the carpet, health, magic, creatures and motes.
local function flight_lines()
  local c = game.carpet
  local lines = flight_hud.lines(c, world.ground_height(c.pos), input.mode(), engine.info(), world.time())
  local parts = { health.hud_lines(game.health), survival.hud_lines(game.survival), pack.hud_lines(game.pack),
    bag.hud_lines(game.bag), condition.hud_lines(game.condition), cloak.hud_lines(game.cloak),
    artificing.hud_lines(game.artifacts), game.ritual.active and { ritual.line(game.ritual, game.pack) } or {},
    summoning.hud_lines(game.summoning, world.time().time), disciplines.hud_lines(game.disciplines),
    codex.hud_lines(game.codex),
    magic.hud_lines(game.magic), fauna.hud_lines(game.fauna), motes.hud_lines(game.motes) }
  for _, more in ipairs(parts) do
    for _, line in ipairs(more) do lines[#lines + 1] = line end
  end
  if game.channeling then lines[#lines + 1] = "Channeling" end
  for _, line in ipairs(modapi.hud_lines(game.mods)) do lines[#lines + 1] = line end
  if game.saga then
    local at = (game.walker and game.walker.pos) or c.pos
    for _, line in ipairs(saga.hud_lines(game.saga, at)) do
      lines[#lines + 1] = line
    end
    for _, m in ipairs(require("story.narrative").data().circle) do -- the next of the Circle to find
      local site = game.saga.N.sites[m.id]
      if site and not game.saga.N.remnants[m.id] then
        local marker = access.marker(site.name, require("people.npcs").directions(at, site.pos))
        if marker then lines[#lines + 1] = marker end
        break
      end
    end
  elseif game.story then
    for _, line in ipairs(prologue.hud_lines(game.story)) do lines[#lines + 1] = line end
  end
  local w = game.walker
  local place = game.world_places and placeplay.line(game.world_places, w and w.pos or c.pos)
  if place then lines[#lines + 1] = place end
  if w then
    lines[#lines + 1] = "On foot" .. (w.on_ice and ", on the ice" or "") .. (w.blocked and " - too deep to wade" or "")
  elseif walker.can_step_off(c, FOOT) then
    lines[#lines + 1] = "Low and slow: E to step off"
  end
  return lines
end

local function home_frame()
  local t, h = game.tent, game.home_place
  local floor = world.move(t.pos, 0, 0, 0, 400)
  local o = world.to_local(floor)
  local i = 1
  for n, r in ipairs(home.rooms()) do
    if r.id == h.room then i = n end
  end
  local turn = (i - 1) * math.pi / 2
  local eye = { o[1] - math.sin(turn) * 2.5, o[2] + 1.6, o[3] + math.cos(turn) * 2.5 }
  render.camera({ eye = eye, target = { o[1] + math.sin(turn) * 3, o[2] + 1.4, o[3] - math.cos(turn) * 3 },
    up = { 0, 1, 0 }, fov = 70, near = 0.1, far = 2000 })
  render.draw_model(assets.room, { position = o, rotation = { 0, 0, 0 } })
  render.draw_model(assets.carpet, { position = { o[1], o[2] + 0.02, o[3] }, rotation = { 0, 90 - math.deg(turn), 0 } })
  if home.offers(h, "cook") then
    render.draw_sprite(assets.flame, { o[1] + math.sin(turn) * 2.6, o[2] + 0.5, o[3] - math.cos(turn) * 2.6 }, 0.7)
  end
  local lines = home.hud_lines(h)
  if home.offers(h, "brew") then lines[#lines + 1] = alchemy.line(alchemy.recipes()[h.recipe or 1], game.pack) end
  if home.offers(h, "read") then
    lines[#lines + 1] = "Study: " .. disciplines.LIST[h.study or 1] .. " - K to study two hours, . for the next"
  end
  if home.offers(h, "craft") then
    local nc = #crafting.crafts()
    local w = h.work or 1
    lines[#lines + 1] = w <= nc and crafting.line(crafting.crafts()[w], game.pack)
      or artificing.line(artificing.enchantments()[w - nc], game.pack, game.magic.caster.mage)
  end
  for _, more in ipairs({ health.hud_lines(game.health), survival.hud_lines(game.survival), pack.hud_lines(game.pack),
    bag.hud_lines(game.bag), condition.hud_lines(game.condition), codex.hud_lines(game.codex),
    disciplines.hud_lines(game.disciplines) }) do
    for _, line in ipairs(more) do lines[#lines + 1] = line end
  end
  draw_hud(lines)
end

local function flight_frame()
  if game.tent and game.tent.inside then
    home_frame()
    return
  end
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
    render.camera({ eye = eye, target = target, up = world.up_at(game.walker.pos), fov = access.fov(70), near = 0.1,
      far = 30000 })
  else
    local eye, target = camera.view(game.chase, p, c.heading, up, dt)
    render.camera({ eye = eye, target = target, up = up, fov = access.fov(70) - 5, near = 0.5, far = 30000 })
  end
  if game.tent then
    local tp = world.to_local(game.tent.pos)
    render.draw_model(assets.tent, { position = tp, rotation = { 0, 180 - math.deg(game.tent.heading), 0 } })
  elseif not game.cloak.worn then
    render.draw_model(assets.carpet, { position = p, rotation = carpet.rotation(c) })
  end
  placeplay.draw(game.world_places, game.walker and game.walker.pos or c.pos, assets.person)
  if game.story and game.story.azhdar and game.story.beat == "arrival" then -- the Ash Weaver on his dark carpet
    render.draw_model(assets.carpet, { position = world.to_local(game.story.azhdar), rotation = { 0, 45, 0 },
      scale = 1.2 })
  end
  local sights = magic.sights(game.magic)
  if artificing.working(game.artifacts, "lantern_of_light") then -- (the lantern in the mage's hand)
    local at = game.walker and world.move(game.walker.pos, game.walker.heading, 0.4, 0.3, 1.2)
      or world.move(c.pos, c.heading, 0.5, 0, 1)
    render.draw_sprite(assets.mote, world.to_local(at), 1.2)
  end
  if game.ritual.active then -- (the circle's points glow while it is channelled)
    for k = 0, 7 do
      local pt = world.move(game.ritual.active.pos, k * math.pi / 4, ritual.RADIUS, 0, 0.3)
      render.draw_sprite(assets.mote, world.to_local(pt), 0.8)
    end
  end
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
  text.lines(assets.font, creation.lines(game.creation), hud.color)
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
    text.lines(assets.font, hud.lines(s, input.mode(), info, game.credits), hud.color)
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

-- The extension API for mods (docs/modding/game-api.md), shared through _G.
game.mods = modapi.new({
  pack = function() return game.pack end,
  say = function(message) if game.pack then pack.say(game.pack, message) end end,
  player = function()
    local c, w = game.carpet, game.walker
    local pos = (w and w.pos) or (c and c.pos)
    local sky = pos and world.sky(pos)
    return { pos = pos, on_foot = w ~= nil, night = sky ~= nil and sky.daylight < 0.2, time = world.time().time }
  end,
})
_G.zauberer = game.mods.public

-- Exposed for scenario tests.
_G.demo = {
  game = game, assets = assets, state = state, touch = touch, carpet = carpet, controls = controls, coop = coop,
  creation = creation, magic = magic, fauna = fauna, motes = motes, health = health, flight_lines = flight_lines,
  walker = walker, ecology = ecology, plants = plants, smalllife = smalllife, survival = survival,
  pack = pack, bag = bag, condition = condition, cloak = cloak, tent = tent, home = home, harvest = harvest,
  cooking = cooking, codex = codex, alchemy = alchemy, crafting = crafting, artificing = artificing, ritual = ritual,
  summoning = summoning, disciplines = disciplines, placeplay = placeplay, places = require("places.places"),
}
