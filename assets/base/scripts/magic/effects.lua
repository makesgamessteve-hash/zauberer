-- SPDX-License-Identifier: MIT
-- Weave effects through the simulation (concept 3.1: magic pushes the world;
-- every effect is a source or sink of heat, matter, force, light, growth,
-- decay or space; everything after that is the simulation's response).
-- A plan from magic.weave is carried out here: at once, after a delay, or by a
-- projectile when it lands. Lights and wards last for a while.
local weave = require("magic.weave")

local effects = {}

local BOLT_SPEED = 40 -- m/s
local BOLT_REACH = 1.5 -- metres: a bolt this close to a creature strikes it
-- What a bolt does to a creature it strikes, per mana: fire burns, water and
-- stone strike.
local BOLT_HARM = { fire = { "heat", 6 }, water = { "impact", 2 }, earth = { "impact", 8 } }

-- What one mana does in each domain (data/weaves.lua).
local function rate(color, what)
  for _, c in ipairs(weave.data().colors) do
    if c.id == color then return c.per_mana[what] end
  end
end
local JOULES = rate("fire", "joules")
local IMPULSE = rate("storm", "impulse")
local LITRES = rate("water", "cubic_metres")
local HEALTH = rate("life", "health")

function effects.new()
  return { projectiles = {}, delayed = {}, lights = {}, wards = {}, heard = {}, time = 0 }
end

-- A place moved by east and north metres.
local function offset(pos, east, north)
  local p = world.move(pos, math.atan(east, north), math.sqrt(east * east + north * north), 0)
  return p
end

local function on_ground(pos)
  local p = world.move(pos, 0, 0, 0)
  p.alt = world.ground_height(p)
  return p
end

-- The direction of a heading and pitch as { east, north, up }.
local function aim_vector(heading, pitch)
  local c = math.cos(pitch or 0)
  return { east = math.sin(heading) * c, north = math.cos(heading) * c, up = math.sin(pitch or 0) }
end

local function ward(state, kind, p)
  local seconds = 30 * p.power / 10 * p.extent.duration
  state.wards[kind] = math.max(state.wards[kind] or 0, state.time + seconds)
  return { ward = kind, seconds = seconds }
end

-- Pushes the bodies around p.at away from a place (render coordinates are
-- x east, y up, z south).
local function push_bodies(p, from, impulse)
  local ids = sim.bodies_near(p.at, p.radius + 2)
  local f = world.to_local(from)
  for _, id in ipairs(ids) do
    local b = world.to_local(sim.body(id).pos)
    local east, north = b[1] - f[1], -(b[3] - f[3])
    local len = math.sqrt(east * east + north * north)
    if len < 0.1 then east, north, len = 0, 1, 1 end
    local share = impulse / #ids
    sim.push(id, { east = east / len * share, north = north / len * share, up = 0.3 * share })
  end
  return #ids
end

-- Life and death for the creatures within reach (the player hears the event).
local function touch_creatures(at, radius, harm, amount)
  local n = 0
  for _, id in ipairs(sim.creatures_near(at, radius)) do
    local c = sim.creature(id)
    if c and c.alive then
      sim.hurt(id, harm, amount)
      n = n + 1
    end
  end
  return n
end

-- Handlers by "verb:colour". p: at (target), radius, power (mana), extent,
-- origin, heading, pitch, ctx (caster, carpet, state).
local H = {}

H["create:fire"] = function(p)
  sim.add_heat(p.at, p.radius, p.power * JOULES)
  return { heat = p.power * JOULES }
end
H["unmake:fire"] = function(p)
  return { put_out = sim.cool(p.at, p.radius, p.power * JOULES * 4) }
end
H["shape:fire"] = function(p) -- melt: heat in a small spot
  sim.add_heat(p.at, math.min(p.radius, 1.5), p.power * JOULES * 1.5)
  return { heat = p.power * JOULES * 1.5 }
end
H["sense:fire"] = function(p)
  local hottest, at = -math.huge, nil
  for dx = -2, 2 do
    for dy = -2, 2 do
      local q = offset(p.at, dx * p.radius / 2, dy * p.radius / 2)
      local c = sim.fire_at(q)
      if c.loaded and c.temperature > hottest then hottest, at = c.temperature, q end
    end
  end
  return { sensed = "heat", temperature = hottest, pos = at }
end
H["ward:fire"] = function(p, state) return ward(state, "fire", p) end

H["create:storm"] = function(p) -- a spark: it jolts what it touches and lights dry fuel
  return { sparked = sim.ignite(p.at, 1), creatures = touch_creatures(p.at, math.max(p.radius, 2), "magic",
    p.power * 3) }
end
-- A gust along a line pushes what stands in its path the way it blows.
local function push_along(p, impulse)
  local found, ids = {}, {}
  local reach = world.distance(p.origin, p.at)
  for d = 0, reach + 2, 2 do
    local q = world.move(p.origin, p.heading, d * math.cos(p.pitch), 0, d * math.sin(p.pitch))
    for _, id in ipairs(sim.bodies_near(q, p.radius + 1.5)) do
      if not found[id] then
        found[id] = true
        ids[#ids + 1] = id
      end
    end
  end
  local dir = aim_vector(p.heading, p.pitch)
  for _, id in ipairs(ids) do
    local share = impulse / #ids
    sim.push(id, { east = dir.east * share, north = dir.north * share, up = (dir.up + 0.2) * share })
  end
  return #ids
end

H["move:storm"] = function(p)
  if p.form == "line" or p.form == "wave" then
    return { pushed = push_along(p, p.power * IMPULSE) }
  end
  return { pushed = push_bodies(p, p.origin, p.power * IMPULSE) }
end
H["sense:storm"] = function(p)
  local w = sim.wind_at(p.at)
  return { sensed = "wind", east = w.east, north = w.north }
end
H["ward:storm"] = function(p, state) return ward(state, "storm", p) end

H["create:light"] = function(p, state)
  local seconds = 20 * p.power / 10 * p.extent.duration
  local orb = on_ground(p.at)
  orb.alt = orb.alt + 2
  state.lights[#state.lights + 1] = { pos = orb, until_time = state.time + seconds, strength = p.power }
  return { light = seconds }
end

H["create:life"] = function(p, state)
  local amount = p.power * HEALTH
  local radius = math.max(p.radius, 2)
  state.heard[#state.heard + 1] = { kind = "heal", at = p.at, amount = amount, radius = radius }
  return { healed = amount, creatures = touch_creatures(p.at, radius, "magic", -amount) }
end
H["unmake:life"] = function(p, state)
  local gone = world.remove_plants(p.at, math.min(p.radius, 64))
  local radius = math.max(p.radius, 2)
  local amount = p.power * HEALTH
  state.heard[#state.heard + 1] = { kind = "wither", at = p.at, amount = amount, radius = radius }
  return { withered = gone, creatures = touch_creatures(p.at, radius, "magic", amount) }
end
H["sense:life"] = function(p)
  return { sensed = "life", plants = #world.plants(p.at, math.min(p.radius * 2, 256)) }
end

H["create:water"] = function(p)
  local m3 = p.power * LITRES
  sim.add_water(p.at, math.min(p.radius, 64), m3)
  return { water = m3 }
end
H["unmake:water"] = function(p)
  return { dried = sim.remove_water(p.at, math.min(p.radius, 64), p.power * LITRES * 10) }
end
H["shape:water"] = function(p)
  if sim.water_at(p.at).frozen then
    return { thawed = sim.thaw(p.at, math.min(p.radius, 64)) }
  end
  local seconds = math.min(60 * p.power / 5 * p.extent.duration, 86400)
  return { frozen = sim.freeze(p.at, math.min(p.radius, 64), seconds), seconds = seconds }
end
H["sense:water"] = function(p)
  for r = 0, 60, 6 do
    for a = 0, 5.9, 0.8 do
      local q = world.move(p.at, a, r, 0)
      local c = sim.water_at(q)
      if c.loaded and (c.fixed or c.depth > 0.02) then
        return { sensed = "water", distance = r, pos = q }
      end
    end
  end
  return { sensed = "water" }
end
H["ward:water"] = function(p, state) return ward(state, "water", p) end

H["move:earth"] = function(p) -- raise the ground: a mound, or a wall across the aim
  local amount = math.min(8, 0.3 * p.power)
  local changed = 0
  if p.form == "square" then
    for k = -2, 2 do
      changed = changed + world.deform(world.move(p.at, p.heading, 0, k * p.radius * 0.6), p.radius * 0.6, amount)
    end
  else
    changed = world.deform(p.at, math.max(p.radius, 0.5), amount)
  end
  return { raised = amount, chunks = changed }
end
H["unmake:earth"] = function(p)
  local amount = math.min(8, 0.3 * p.power)
  return { dug = amount, chunks = world.deform(p.at, math.max(p.radius, 0.5), -amount) }
end
H["create:earth"] = function(p)
  local stone = on_ground(p.at)
  local radius = math.min(1.5, 0.15 * p.power ^ (1 / 3) + 0.1)
  stone.alt = stone.alt + radius + 0.5
  return { stone = sim.spawn { shape = "sphere", material = "rock", pos = stone, radius = radius } }
end
H["shape:earth"] = function(p)
  local n = 0
  for dx = -1, 1 do
    for dy = -1, 1 do
      world.set_surface(offset(p.at, dx * p.radius / 2, dy * p.radius / 2), "mud")
      n = n + 1
    end
  end
  return { mud = n }
end
H["ward:earth"] = function(p, state) return ward(state, "earth", p) end

H["move:void"] = function(p) -- blink
  local carpet = p.ctx.carpet
  if not carpet then return { fizzled = "nothing to move" } end
  local metres = math.min(40, 2 * p.power)
  carpet.pos = world.move(carpet.pos, p.heading, metres, 0)
  return { blinked = metres }
end
H["sense:void"] = function(p)
  local ley = mana.ley(p.at)
  return { sensed = "ley", line_distance = ley.line_distance, nexus_distance = ley.nexus_distance }
end
H["unmake:void"] = function(p, state) -- dispel lasting effects nearby
  local kept, gone = {}, 0
  for _, l in ipairs(state.lights) do
    if world.distance(l.pos, p.at) > p.radius + 2 then kept[#kept + 1] = l else gone = gone + 1 end
  end
  state.lights = kept
  return { dispelled = gone }
end

H["create:decay"] = function(p)
  mana.release(p.at, "decay", p.power)
  return { rotted = world.remove_plants(p.at, math.min(p.radius * 0.5, 64)) }
end
H["unmake:decay"] = function(p, state)
  state.heard[#state.heard + 1] = { kind = "purify", at = p.at, amount = p.power }
  return { purified = p.power }
end

-- Two colours with their own effect: the pair's handler, else each colour
-- with its share of the power.
local PAIRS = {}
PAIRS["move:fire+storm"] = function(p, state) -- an exploding fireball: heat and a blast
  local r = H["create:fire"](p, state)
  r.pushed = push_bodies(p, p.at, p.power * IMPULSE * 0.5)
  return r
end
PAIRS["move:fire+water"] = function(p) -- steam blast
  sim.emit(p.at, "steam", p.power * 0.5)
  sim.add_heat(p.at, p.radius, p.power * JOULES * 0.5)
  return { steam = p.power * 0.5 }
end
PAIRS["create:life+water"] = function(p, state) -- healing rain
  local r = H["create:water"](p, state)
  r.healed = H["create:life"](p, state).healed
  return r
end
PAIRS["shape:earth+water"] = function(p, state) return H["shape:earth"](p, state) end

local function key_of(verb, colors)
  return verb .. ":" .. table.concat(colors, "+")
end

-- Runs a weave's effect now at p.at (or, for bolts, as a projectile).
local function run(state, p)
  local w = p.weave
  local colors = {}
  for _, c in ipairs(w.colors) do colors[#colors + 1] = c end
  table.sort(colors, function(a, b)
    local order = { fire = 1, storm = 2, light = 3, life = 4, water = 5, earth = 6, void = 7, decay = 8 }
    return order[a] < order[b]
  end)
  local carried = (w.form == "line" or w.form == "zigzag") and w.verb == "move"
    and (colors[1] == "fire" or colors[1] == "water" or colors[1] == "earth") and not p.misfire_here
  if carried then -- a bolt: the effect waits for it to land
    local dir = aim_vector(p.heading, p.pitch)
    local speed = BOLT_SPEED
    local mass = colors[1] == "earth" and math.min(100, 2 * p.power) or 0.5
    local id = sim.launch(p.origin, { east = dir.east * speed, north = dir.north * speed, up = dir.up * speed }, 0.15,
      mass)
    state.projectiles[#state.projectiles + 1] = { id = id, plan = p }
    return { launched = id }
  end
  local pair = PAIRS[key_of(w.verb, colors)]
  if pair then return pair(p, state) end
  local share = #colors == 2 and { 0.6, 0.4 } or { 1 }
  local out = {}
  for i, c in ipairs(colors) do
    local h = H[w.verb .. ":" .. c]
    if h then
      local part = {}
      for k, v in pairs(p) do part[k] = v end
      part.power = p.power * share[i]
      for k, v in pairs(h(part, state)) do out[k] = v end
    end
  end
  if next(out) == nil then
    out.fizzled = "the weave finds no hold" -- not yet known to the world (bind, call, ...)
  end
  return out
end

-- When a bolt lands: its colour's effect where it struck.
local function land(state, proj, at)
  local p = proj.plan
  local landed = {}
  for k, v in pairs(p) do landed[k] = v end
  landed.at = at
  local c = p.weave.colors
  if c[1] == "earth" then return { struck = at } end -- the stone itself did the work
  landed.weave = { verb = "create", colors = c, form = "tap", power = p.power, modifiers = {} }
  landed.radius = 1 + p.power / 10
  if c[1] == "water" then
    sim.cool(at, landed.radius, p.power * JOULES) -- a jet of water douses
  end
  return run(state, landed)
end

-- A bolt in flight striking a living creature near it; returns its id.
local function strike(proj, pos)
  for _, id in ipairs(sim.creatures_near(pos, BOLT_REACH)) do
    local c = sim.creature(id)
    if c and c.alive then
      local p = proj.plan
      local how = BOLT_HARM[p.weave.colors[1]] or { "magic", 3 }
      sim.hurt(id, how[1], p.power * how[2])
      return id
    end
  end
  return nil
end

-- Carries out a resolved plan. ctx: origin (where the weave leaves the hand),
-- heading and pitch (the aim), target (the aimed place on the ground or a
-- body), caster, carpet.
function effects.apply(state, plan, ctx)
  local p = { weave = plan.weave, power = plan.power, extent = plan.extent, form = plan.weave.form,
    radius = plan.extent.radius, origin = ctx.origin, heading = ctx.heading or 0, pitch = ctx.pitch or 0, ctx = ctx,
    misfire = plan.misfire }
  local at = ctx.target or ctx.origin
  if plan.weave.form == "tap" then at = ctx.origin end
  if plan.misfire == "backfire" then
    at, p.misfire_here = ctx.origin, true
  elseif plan.misfire == "stray" then
    at = offset(at, plan.offset[1], plan.offset[2])
  end
  p.at = at
  if plan.delay and plan.delay > 0 then
    state.delayed[#state.delayed + 1] = { at_time = state.time + plan.delay, p = p }
    return { delayed = plan.delay }
  end
  return run(state, p)
end

-- Time passes: delayed weaves go off, bolts land, lights and wards fade.
-- Returns the results of what happened this tick.
function effects.tick(state, dt)
  state.time = state.time + dt
  local happened = {}
  local waiting = {}
  for _, d in ipairs(state.delayed) do
    if d.at_time <= state.time then
      happened[#happened + 1] = run(state, d.p)
    else
      waiting[#waiting + 1] = d
    end
  end
  state.delayed = waiting
  local flying = {}
  for _, f in ipairs(state.projectiles) do
    local s = sim.projectile(f.id)
    local struck = s and s.flying and strike(f, s.pos)
    if struck then -- it hit a creature on its way
      sim.remove_body(f.id)
      local r = land(state, f, s.pos)
      r.struck_creature = struck
      happened[#happened + 1] = r
    elseif s and not s.flying then
      happened[#happened + 1] = land(state, f, s.hit and s.hit.pos or s.pos)
    elseif s then
      flying[#flying + 1] = f
    end
  end
  state.projectiles = flying
  local lit = {}
  for _, l in ipairs(state.lights) do
    if l.until_time > state.time then lit[#lit + 1] = l end
  end
  state.lights = lit
  for k, t in pairs(state.wards) do
    if t <= state.time then state.wards[k] = nil end
  end
  return happened
end

-- Whether a ward of a kind ("fire", "water", "storm", "earth") is up.
function effects.warded(state, kind)
  return (state.wards[kind] or 0) > state.time
end

-- Events for other systems (healing, withering, purifying), taken once.
function effects.take_events(state)
  local out = state.heard
  state.heard = {}
  return out
end

effects.name = weave.name
return effects
