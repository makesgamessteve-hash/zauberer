-- SPDX-License-Identifier: MIT
-- Small life (concept 5.12): fungi, microbes and insects as fields, rates and
-- swarms rather than creatures.
--   Fungi: mushrooms fruit in 32 m patches by their habitat (forest, open
--   land, dead trees), the season and how wet the ground is; picked patches
--   fruit again the next day.
--   Microbes: food spoils faster when warm and wet and slower when cooked or
--   dried; water is dirty in swamps, still pools and near carrion, salty in
--   the sea; carcasses rot away in a few days.
--   Insects: swarms of bees by flowers on spring and summer days, mosquitoes
--   over wet ground at dusk and on warm nights, fireflies on summer nights and
--   butterflies over sunny meadows, drawn as small sprites. Mosquitoes bite;
--   bees pollinate.
local plants = require("life.plants")

local smalllife = {}

smalllife.DAY = 86400
smalllife.PATCH = 32          -- metres: a fungus patch
smalllife.ROT_DAYS = 3        -- game days a carcass lasts
smalllife.LOOK_EVERY = 2      -- seconds between looks for swarms
smalllife.SWARM_RANGE = 40    -- metres around the player where swarms are
smalllife.BITE_RANGE = 3      -- metres within which mosquitoes bite

local fungi -- read on first use

function smalllife.fungi()
  if not fungi then
    local text = assert(vfs.read("data/fungi.lua"))
    fungi = assert(load(text, "=data/fungi.lua", "t", {}))()
  end
  return fungi
end

local function within(w, s)
  if w[2] - w[1] >= 4 then return true end
  return (s - w[1]) % 4 <= w[2] - w[1]
end

-- A number 0 .. 1 from a key and a day.
local function roll(key, day)
  local h = 2166136261
  for k = 1, #key do h = ((h ~ key:byte(k)) * 16777619) % 4294967296 end
  h = (h ~ (day * 2654435761 % 4294967296)) * 2246822519 % 4294967296
  return (h ~ (h >> 13)) / 4294967296
end

function smalllife.new(saved)
  local state = { picked = {}, carcasses = {}, swarms = {}, since = 0, time = 0 }
  if type(saved) == "table" then
    for k, v in pairs(saved.picked or {}) do state.picked[k] = v end
    for k, v in pairs(saved.carcasses or {}) do state.carcasses[tostring(k)] = v end
  end
  return state
end

function smalllife.snapshot(state, now)
  local out = { picked = {}, carcasses = {} }
  local today = math.floor(now / smalllife.DAY)
  for k, day in pairs(state.picked) do
    if day >= today then out.picked[k] = day end
  end
  for k, died in pairs(state.carcasses) do out.carcasses[k] = died end
  return out
end

-- The patch around pos: its key, centre and what grows there.
local function patch(pos)
  local px, py = math.floor(pos.lx / smalllife.PATCH), math.floor(pos.ly / smalllife.PATCH)
  local key = world.chunk_key(pos) .. ":" .. px .. ":" .. py
  local centre = world.position(pos.face, pos.x, pos.y, (px + 0.5) * smalllife.PATCH, (py + 0.5) * smalllife.PATCH)
  return key, centre
end

-- How wet a place is, 0 .. 1: rain now and water held in the ground.
local function wetness(pos)
  local w = sim.water_at(pos)
  local held = w.loaded and w.capacity > 0 and w.soaked / w.capacity or 0
  local rain = (sim.weather().rain or 0) > 0.2 and 0.5 or 0
  return math.min(held + rain, 1)
end

-- The habitats of a patch, 0 .. 1 each: forest, open, deadwood.
local function habitats(centre)
  local trees, snags, ground = 0, 0, 0
  for _, p in ipairs(world.plants(centre, smalllife.PATCH * 0.7)) do
    if p.species == "snag" then
      snags = snags + 1
    elseif p.layer == "tree" then
      trees = trees + 1
    else
      ground = ground + 1
    end
  end
  local forest = math.min(trees / 12, 1)
  return { forest = forest, open = math.min(ground / 40, 1) * (1 - forest), deadwood = math.min(snags, 1) }
end

-- The mushrooms of the patch at pos now: { fungus, item, count }.
function smalllife.mushrooms(state, pos, now)
  local key, centre = patch(pos)
  local day = math.floor(now / smalllife.DAY)
  if state.picked[key] == day then return {}, key end
  local season = world.sky(centre).season
  local wet = 0.3 + 0.7 * wetness(centre)
  local where = habitats(centre)
  local out = {}
  for _, f in ipairs(smalllife.fungi()) do
    if within(f.season, season) then
      local autumn = (season >= 2 and season < 3) and 1.5 or 1
      local n = math.floor(f.density * (where[f.habitat] or 0) * wet * autumn * roll(key .. f.id, day) + 0.5)
      if n > 0 then out[#out + 1] = { fungus = f.id, item = f.item, count = n } end
    end
  end
  return out, key
end

-- Picks the mushrooms of the patch at pos: { item = count } and what was
-- found ({ fungus, item, count }), or nil.
function smalllife.pick(state, pos, now)
  local found, key = smalllife.mushrooms(state, pos, now)
  if #found == 0 then return nil end
  state.picked[key] = math.floor(now / smalllife.DAY)
  local out = {}
  for _, m in ipairs(found) do out[m.item] = (out[m.item] or 0) + m.count end
  return out, found
end

-- Microbes on food: load 0 (fresh) .. 1 (rotten) after hours at a
-- temperature (deg C), wet or dry, slowed by how it was kept (1 fresh, 0.5
-- cooked, 0.1 dried or smoked). Growth doubles every 10 degrees.
function smalllife.spoil(load, hours, temperature, wet, kept)
  local rate = 0.16 * 2 ^ ((math.min(temperature, 45) - 20) / 10) * (wet and 1.5 or 1) * (kept or 1)
  if temperature <= 0 then rate = 0 end
  local l = math.min(math.max(load, 0.01), 0.999) -- (fresh food carries a few microbes)
  return 1 / (1 + (1 / l - 1) * math.exp(-math.min(rate * hours, 700))) -- logistic: levels off at 1
end

smalllife.SPOILT = 0.5 -- load past which food makes one sick
smalllife.ROTTEN = 0.9 -- past which it is not food any more

local WET = { marsh = true, swamp_forest = true, peat_bog = true, mangroves = true, salt_marsh = true,
  floodplain = true, mirefen = true, drowned_vale = true, estuary = true }

-- Whether a biome (by the name world.biome gives, "swamp forest") is a
-- wetland, where all water is swamp water.
function smalllife.wet_biome(name)
  return WET[(name or ""):gsub(" ", "_")] or false
end

-- The water at a place: kind ("sea", "lake", "river", "pool", "swamp" or
-- nil), how dirty (0 .. 1) and whether it is salty.
function smalllife.water(pos)
  local kind = world.water(pos)
  local sim_water = sim.water_at(pos)
  local out
  if kind == "ocean" then
    out = { kind = "sea", dirty = 0.2, salty = true }
  elseif kind == "lake" or kind == "river" then
    out = { kind = kind, dirty = kind == "river" and 0.1 or 0.2, salty = false }
  elseif sim_water.loaded and sim_water.depth > 0.02 and not sim_water.frozen then
    out = { kind = "pool", dirty = 0.4, salty = false }
  else
    return nil
  end
  if smalllife.wet_biome(world.biome(pos)) then
    out.kind, out.dirty = out.kind == "sea" and "sea" or "swamp", math.max(out.dirty, 0.8)
  end
  -- Carrion fouls the water near it.
  local ground = world.move(pos, 0, 0, 0)
  ground.alt = world.ground_height(pos)
  for _, id in ipairs(sim.creatures_near(ground, 20)) do
    local c = sim.creature(id)
    if c and not c.alive then out.dirty = math.min(out.dirty + 0.4, 1) end
  end
  return out
end

-- ---- Insects ----

-- What swarms a place has now, from the hour, the season, the warmth and what
-- grows there: kind -> how many.
local function swarm_kinds(pos)
  local sky = world.sky(pos)
  local light, season = sky.daylight, sky.season
  local warm = (sim.weather().temperature or 15) > 12
  local flowers, reeds, open = 0, 0, 0
  for _, p in ipairs(world.plants(pos, 25)) do
    -- (grass is pollinated by the wind)
    if p.species ~= "grass" and plants.stage(p, season).flowers then flowers = flowers + 1 end
    if p.species == "reeds" then reeds = reeds + 1 end
    if p.layer == "ground" then open = open + 1 end
  end
  local water = sim.water_at(pos)
  local wet = reeds > 3 or smalllife.wet_biome(world.biome(pos)) or (water.loaded and water.depth > 0.05)
  local growing = season >= 0.2 and season < 2.4
  local summer = season >= 0.8 and season < 1.8
  local out = {}
  if light > 0.4 and flowers > 0 then out.bee = 8 end
  if light > 0.5 and growing and flowers > 0 and open > 20 then out.butterfly = 4 end
  if wet and warm and growing and light < 0.5 then out.mosquito = 14 end
  if light < 0.1 and summer and open > 10 then out.firefly = 10 end
  return out
end

local function new_particle(rng)
  return { x = (rng() - 0.5) * 6, y = 0.5 + rng() * 2, z = (rng() - 0.5) * 6, vx = 0, vy = 0, vz = 0 }
end

-- Once in a while: swarms where the player is now (anchored by the player's
-- position at the time); every tick: the particles wander about their anchor.
function smalllife.tick(state, dt, player_pos, now)
  state.time = state.time + dt
  state.since = state.since + dt
  -- Carcasses rot away.
  for key, died in pairs(state.carcasses) do
    if now - died > smalllife.ROT_DAYS * smalllife.DAY then
      sim.remove_creature(tonumber(key))
      state.carcasses[key] = nil
    end
  end
  if state.since >= smalllife.LOOK_EVERY then
    state.since = 0
    local kinds = swarm_kinds(player_pos)
    local keep = {}
    for _, sw in ipairs(state.swarms) do
      if kinds[sw.kind] and world.distance(sw.anchor, player_pos) < smalllife.SWARM_RANGE then
        keep[#keep + 1] = sw
        kinds[sw.kind] = nil
      end
    end
    local seed = math.floor(state.time * 1000) % 2147483647
    local function rng()
      seed = (seed * 1103515245 + 12345) % 2147483648
      return seed / 2147483648
    end
    for kind, n in pairs(kinds) do
      local anchor = world.move(player_pos, rng() * 2 * math.pi, 4 + rng() * 10, 0)
      anchor.alt = world.ground_height(anchor)
      local sw = { kind = kind, anchor = anchor, particles = {} }
      for k = 1, n do sw.particles[k] = new_particle(rng) end
      keep[#keep + 1] = sw
    end
    state.swarms = keep
  end
  for _, sw in ipairs(state.swarms) do
    local jitter = sw.kind == "mosquito" and 6 or (sw.kind == "firefly" and 1 or 3)
    for _, p in ipairs(sw.particles) do
      local t = state.time + p.x * 3.1
      p.vx = p.vx * 0.9 + (math.sin(t * 2.3) * jitter - p.x * 0.6) * dt
      p.vz = p.vz * 0.9 + (math.cos(t * 1.7) * jitter - p.z * 0.6) * dt
      p.vy = p.vy * 0.9 + (math.sin(t * 1.3) * 0.8 - (p.y - 1.4) * 0.8) * dt
      p.x, p.y, p.z = p.x + p.vx * dt, p.y + p.vy * dt, p.z + p.vz * dt
    end
  end
end

-- The insects to draw: { kind, pos = local x, y, z }.
function smalllife.sights(state)
  local out = {}
  for _, sw in ipairs(state.swarms) do
    local a = world.to_local(sw.anchor)
    for _, p in ipairs(sw.particles) do
      out[#out + 1] = { kind = sw.kind, pos = { a[1] + p.x, a[2] + p.y, a[3] + p.z } }
    end
  end
  return out
end

-- How many swarms of a kind are about.
function smalllife.swarms(state, kind)
  local n = 0
  for _, sw in ipairs(state.swarms) do n = n + (sw.kind == kind and 1 or 0) end
  return n
end

-- Mosquito bites a second at pos (the mosquitoes within BITE_RANGE).
function smalllife.bites(state, pos)
  local n = 0
  for _, sw in ipairs(state.swarms) do
    if sw.kind == "mosquito" and world.distance(sw.anchor, pos) < smalllife.BITE_RANGE + 3 then
      n = n + #sw.particles * 0.05
    end
  end
  return n
end

-- Pollination at pos, 0 .. 1: bees about nearby.
function smalllife.pollination(state, pos)
  for _, sw in ipairs(state.swarms) do
    if sw.kind == "bee" and world.distance(sw.anchor, pos) < 60 then return 1 end
  end
  return 0
end

-- Remembers a death so the carcass rots away in time.
function smalllife.died(state, id, now)
  state.carcasses[tostring(id)] = now
end

return smalllife
