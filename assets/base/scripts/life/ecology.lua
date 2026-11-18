-- SPDX-License-Identifier: MIT
-- Ecology (concept 5.13): every species has a count per chunk that grows
-- logistically towards a carrying capacity set by the chunk's habitat, is
-- eaten down by the predators of the food web (data/creatures.lua: eats), and
-- flows to neighbouring chunks where the species is thinner. Counts catch up
-- lazily when a chunk is looked at again.
--
-- Virtualisation: as chunks come within the active tier around the player,
-- individuals spawn from the counts (deterministic places per chunk, species,
-- slot and day; herds and packs as groups); when the chunks leave, the
-- individuals fold back into the counts, the dead taken off.
local fauna = require("life.fauna")

local ecology = {}

ecology.DAY = 86400
ecology.YEAR = 48              -- game days
ecology.MAX_STEPS = 96         -- catch-up steps (two years) at most
ecology.CHUNK_KM2 = 0.256 * 0.256
ecology.LOOK_EVERY = 1.0       -- seconds between looks at the chunks around the player
ecology.MIGRATION = 3.0        -- per year: how fast a species evens out between chunks
ecology.START = 0.6            -- share of the capacity a chunk starts with
ecology.PREDATOR_FLOOR = 0.1   -- share of their capacity predators keep without prey

ecology.CLASSES = { "forest", "open", "wetland", "dry", "cold" }

-- Biomes of created worlds as shares of the habitat classes.
local BIOMES = {
  deciduous_forest = { forest = 0.8, open = 0.2 }, mixed_forest = { forest = 0.8, open = 0.2 },
  temperate_rainforest = { forest = 0.9, wetland = 0.1 }, taiga = { forest = 0.7, cold = 0.3 },
  montane_forest = { forest = 0.7, cold = 0.3 },
  rainforest = { forest = 1 }, seasonal_forest = { forest = 0.8, open = 0.2 },
  cloud_forest = { forest = 0.9, cold = 0.1 }, swamp_forest = { forest = 0.5, wetland = 0.5 },
  mangrove = { wetland = 0.8, forest = 0.2 }, mediterranean = { forest = 0.4, open = 0.4, dry = 0.2 },
  steppe = { open = 0.8, dry = 0.2 }, savanna = { open = 0.8, dry = 0.2 }, alpine_meadow = { open = 0.6, cold = 0.4 },
  floodplain = { open = 0.5, wetland = 0.5 },
  marsh = { wetland = 0.9, open = 0.1 }, peat_bog = { wetland = 0.8, cold = 0.2 },
  salt_marsh = { wetland = 0.8, open = 0.2 }, estuary = { wetland = 0.7, open = 0.3 },
  sand_desert = { dry = 1 }, rocky_desert = { dry = 1 },
  salt_flat = { dry = 1 }, xeric_shrubland = { dry = 0.7, open = 0.3 },
  dunes = { dry = 1 }, beach = { open = 0.5, dry = 0.5 }, cliffs = { open = 0.5, cold = 0.2 },
  tundra = { open = 0.5, cold = 0.5 }, polar_desert = { cold = 1 }, ice_sheet = { cold = 1 }, glacier = { cold = 1 },
  scree = { cold = 0.6, open = 0.4 }, volcanic_slopes = { dry = 0.6, open = 0.4 },
  ashlands = { dry = 1 }, stormspires = { open = 0.6, cold = 0.4 }, glasswood = { forest = 1 },
  wildheart = { forest = 1 }, drowned_vale = { wetland = 1 }, deepstone = { cold = 0.5, open = 0.5 },
  hollows = { open = 1 }, mirefen = { wetland = 1 },
}

-- The habitat of a chunk (its centre pos): shares of the classes, from its
-- biome in a created world, else from its plants and ground.
function ecology.habitat(pos)
  local biome = world.biome(pos)
  if biome then
    local shares = BIOMES[biome] or {}
    local out = {}
    for _, c in ipairs(ecology.CLASSES) do out[c] = shares[c] or 0 end
    return out
  end
  local trees, reeds, all = 0, 0, 0
  for _, p in ipairs(world.plants(pos, 128)) do
    all = all + 1
    if p.layer == "tree" then trees = trees + 1 end
    if p.species == "reeds" then reeds = reeds + 1 end
  end
  local kind = world.water(pos)
  local land = (kind == "ocean" or kind == "lake") and 0 or 1
  local surface = world.ground_surface(pos)
  local forest = math.min(trees / 80, 1)
  local wetland = math.min(reeds / 30, 1)
  local dry = (surface == "sand" and all < 40) and 0.8 or 0
  local cold = (surface == "snow" or surface == "ice") and 0.8 or 0
  local open = math.max(1 - forest - wetland - dry - cold, 0)
  return { forest = forest * land, open = open * land, wetland = wetland * land, dry = dry * land, cold = cold * land }
end

-- A species' capacity in a chunk of this habitat: its density where it fits best.
function ecology.capacity(sp, habitat)
  local fit = 0
  for _, c in ipairs(ecology.CLASSES) do fit = fit + (sp.habitat and sp.habitat[c] or 0) * (habitat[c] or 0) end
  return (sp.density or 0) * ecology.CHUNK_KM2 * math.min(fit, 1)
end

-- A record of a chunk first seen at time now.
function ecology.record(species, habitat, now)
  local r = { t = now, habitat = habitat, k = {}, n = {} }
  for _, sp in ipairs(species) do
    r.k[sp.id] = ecology.capacity(sp, habitat)
    r.n[sp.id] = r.k[sp.id] * ecology.START
  end
  return r
end

-- How hard a species is hunted (per year) by the predators in a record.
local function pressure(species, r, prey)
  local q = 0
  for _, pred in ipairs(species) do
    local rate = pred.eats and pred.eats[prey]
    if rate and r.k[pred.id] and r.k[pred.id] > 0 then q = q + rate * r.n[pred.id] / r.k[pred.id] end
  end
  return q
end

-- How well fed a predator is: the share of its prey's capacity that is there.
local function fed(r, pred)
  local have, could = 0, 0
  for prey in pairs(pred.eats or {}) do
    have, could = have + (r.n[prey] or 0), could + (r.k[prey] or 0)
  end
  return could > 0 and have / could or 1
end

-- Advances a record by days (in steps of a day at most, MAX_STEPS at most).
function ecology.advance(species, r, days)
  if days <= 0 then return end
  local steps = math.min(math.ceil(days), ecology.MAX_STEPS)
  local dt = days / steps / ecology.YEAR -- years per step
  for _ = 1, steps do
    local next_n = {}
    for _, sp in ipairs(species) do
      local n, k = r.n[sp.id] or 0, r.k[sp.id] or 0
      if sp.eats then -- predators: their capacity follows their prey
        k = k * (ecology.PREDATOR_FLOOR + (1 - ecology.PREDATOR_FLOOR) * math.min(fed(r, sp), 1))
      end
      local growth = k > 0 and (sp.growth or 0) * n * (1 - n / k) or -n
      local eaten = pressure(species, r, sp.id) * n
      next_n[sp.id] = math.max(math.min(n + (growth - eaten) * dt, r.k[sp.id] or 0), 0)
    end
    r.n = next_n
  end
end

-- Lets species flow between records that are up to date (per year at
-- MIGRATION): from where a species is denser for its capacity to where it is
-- thinner, never above a capacity.
function ecology.migrate(species, records, days)
  local dt = math.min(days / ecology.YEAR * ecology.MIGRATION, 0.5)
  for _, sp in ipairs(species) do
    local total, cap = 0, 0
    for _, r in ipairs(records) do
      total, cap = total + r.n[sp.id], cap + r.k[sp.id]
    end
    if cap > 0 then
      local share = total / cap
      for _, r in ipairs(records) do
        local target = r.k[sp.id] * share
        r.n[sp.id] = math.min(r.n[sp.id] + (target - r.n[sp.id]) * dt, r.k[sp.id])
      end
    end
  end
end

function ecology.new(saved)
  local state = { chunks = {}, live = {}, since = 0, species = nil }
  for key, r in pairs(type(saved) == "table" and saved.chunks or {}) do state.chunks[tostring(key)] = r end
  return state
end

-- The ecology species (those with a density), in id order.
function ecology.species()
  local out = {}
  for id, sp in pairs(fauna.species()) do
    if (sp.density or 0) > 0 then out[#out + 1] = sp end
    sp.id = sp.id or id
  end
  table.sort(out, function(a, b) return a.id < b.id end)
  return out
end

-- The record of the chunk at pos, caught up to now.
function ecology.at(state, pos, now)
  local species = state.species or ecology.species()
  state.species = species
  local key = tostring(world.chunk_key(pos))
  local r = state.chunks[key]
  if not r then
    local centre = world.position(pos.face, pos.x, pos.y, 128, 128)
    r = ecology.record(species, ecology.habitat(centre), now)
    state.chunks[key] = r
  elseif now > r.t then
    ecology.advance(species, r, (now - r.t) / ecology.DAY)
  end
  r.t = math.max(r.t, now)
  return r, key
end

-- A number 0 .. 1 from a chunk key, a species, a slot and a day.
local function roll(key, id, slot, day)
  local h = (tonumber(key) or 0) % 2147483647
  for k = 1, #id do h = (h * 33 + id:byte(k)) % 4294967296 end
  h = (h * 2654435761 + slot * 40503 + day * 9973) % 4294967296
  h = (h ~ (h >> 16)) * 2246822519 % 4294967296
  return (h ~ (h >> 13)) / 4294967296
end

-- Spawns a chunk's individuals from its counts; returns { [id] = list of ids }.
local function spawn(state, r, key, pos, now)
  local day = math.floor(now / ecology.DAY)
  local live = {}
  for _, sp in ipairs(state.species) do
    local g = sp.group and math.max((sp.group[1] + sp.group[2]) / 2, 1) or 1
    local n = r.n[sp.id]
    local groups = math.floor(n / g) + (roll(key, sp.id, 0, day) < (n / g) % 1 and 1 or 0)
    local ids = {}
    for slot = 1, groups do
      local size = 1 -- a herd or pack comes as a group, on average as many as the count
      if sp.group then
        size = sp.group[1] + math.floor(roll(key, sp.id, slot * 7, day) * (sp.group[2] - sp.group[1] + 1))
      end
      local cx, cy = 16 + roll(key, sp.id, slot, day) * 224, 16 + roll(key, sp.id, slot + 100, day) * 224
      for m = 1, size do
        local at = world.position(pos.face, pos.x, pos.y, cx + (roll(key, sp.id, slot * 31 + m, day) - 0.5) * 12,
          cy + (roll(key, sp.id, slot * 37 + m, day) - 0.5) * 12)
        local water = sim.water_at(at)
        if not (water.loaded and water.depth > 0.4 and not water.frozen and not sp.flies) then
          ids[#ids + 1] = sim.spawn_creature(sp.id, at)
        end
      end
    end
    live[sp.id] = ids
  end
  return live
end

-- Folds a chunk's individuals back into its counts: the dead are taken off.
local function fold(state, key, live)
  local r = state.chunks[key]
  for id, ids in pairs(live) do
    local dead = 0
    for _, cid in ipairs(ids) do
      local c = sim.creature(cid)
      if not c or not c.alive then dead = dead + 1 end
      sim.remove_creature(cid)
    end
    if r and r.n[id] then r.n[id] = math.max(r.n[id] - dead, 0) end
  end
end

-- The chunks around pos that are virtualised: its own and its neighbours.
local function around(pos)
  local out = { world.position(pos.face, pos.x, pos.y, 128, 128) }
  for _, p in ipairs(world.neighbors(pos)) do out[#out + 1] = p end
  return out
end

-- Once a second: chunks that came near spawn their individuals, chunks that
-- left fold them back, and the near ones exchange their species.
function ecology.tick(state, dt, player_pos, now)
  if state.paused then return end
  state.since = state.since + dt
  if state.since < ecology.LOOK_EVERY then return end
  local days = (now - (state.looked_at or now)) / ecology.DAY
  state.since, state.looked_at = 0, now
  local wanted, records = {}, {}
  for _, pos in ipairs(around(player_pos)) do
    local r, key = ecology.at(state, pos, now)
    wanted[key] = pos
    records[#records + 1] = r
    if not state.live[key] then state.live[key] = spawn(state, r, key, pos, now) end
  end
  for key, live in pairs(state.live) do
    if not wanted[key] then
      fold(state, key, live)
      state.live[key] = nil
    end
  end
  ecology.migrate(state.species, records, days)
end

-- Pauses (folding every chunk back) or resumes the ecology: for a scene
-- that wants only the animals it places.
function ecology.pause(state, on)
  if on then ecology.fold_all(state) end
  state.paused = on or nil
end

-- Folds every chunk back (leaving flight, saving).
function ecology.fold_all(state)
  for key, live in pairs(state.live) do fold(state, key, live) end
  state.live = {}
end

-- The live individuals of a chunk (by species), if it is virtualised.
function ecology.live(state, pos)
  return state.live[tostring(world.chunk_key(pos))]
end

-- What to keep with the game: the records, with the dead among the live
-- individuals taken off (the living stay counted).
function ecology.snapshot(state)
  local out = { chunks = {} }
  for key, r in pairs(state.chunks) do
    local n = {}
    for id, v in pairs(r.n) do n[id] = v end
    for id, ids in pairs(state.live[key] or {}) do
      for _, cid in ipairs(ids) do
        local c = sim.creature(cid)
        if n[id] and (not c or not c.alive) then n[id] = math.max(n[id] - 1, 0) end
      end
    end
    out.chunks[key] = { t = r.t, habitat = r.habitat, k = r.k, n = n }
  end
  return out
end

return ecology
