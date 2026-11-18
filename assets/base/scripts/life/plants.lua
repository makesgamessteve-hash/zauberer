-- SPDX-License-Identifier: MIT
-- The life cycle of plants (concept 5.12): leaves, flowers and fruit by the
-- local season, each plant a few days either side of its species; the parts
-- that can be taken from it, how many by its size, and when the same plant
-- gives them again. Taking a whole plant cuts it (world.cut_plant): it grows
-- back as the land heals, ground cover in weeks and trees in years. Plants
-- are the world's deterministic placements; only what was taken is kept.
local items = require("life.items")

local plants = {}

plants.DAY = 86400
plants.YEAR = 48 -- game days
plants.KEEP_DAYS = 50 -- records older than every regrowth are dropped from saves

local WHEN = { fruit = true, flowers = true, leaves = true }
local cycles -- species id -> life cycle, read on first use

local function check_window(w, where)
  if w == nil then return true end
  if type(w) ~= "table" or type(w[1]) ~= "number" or type(w[2]) ~= "number" or w[1] < 0 or w[1] >= 4 or
      w[2] < w[1] or w[2] - w[1] > 4 then
    return false, where .. " must be { from, to } in seasons (0 .. 4, at most a year)"
  end
  return true
end

-- Checks life cycles against the items; returns them, or nil and what is wrong.
function plants.check(data, registry)
  if type(data) ~= "table" then return nil, "no life cycles" end
  for id, cycle in pairs(data) do
    local where = "plant " .. tostring(id)
    for _, field in ipairs({ "leaves", "flowers", "fruit" }) do
      local ok, err = check_window(cycle[field], where .. " " .. field)
      if not ok then return nil, err end
    end
    if type(cycle.parts) ~= "table" then return nil, where .. ": no parts" end
    for i, p in ipairs(cycle.parts) do
      local part = where .. " part " .. i
      if not registry[p.item] then return nil, part .. ": unknown item " .. tostring(p.item) end
      if p.when ~= nil and (not WHEN[p.when] or not cycle[p.when]) then
        return nil, part .. ": when must be a stage the plant has (fruit, flowers, leaves)"
      end
      local y = p.yield
      if type(y) ~= "table" or not (y[1] and y[2] and y[1] >= 1 and y[2] >= y[1]) then
        return nil, part .. ": yield must be { least >= 1, most >= least }"
      end
      if not p.whole and (type(p.regrow) ~= "number" or p.regrow <= 0) then
        return nil, part .. ": regrow must be days > 0 (or the part takes the whole plant)"
      end
    end
  end
  return data
end

function plants.cycles()
  if not cycles then
    local text = assert(vfs.read("data/plants.lua"))
    cycles = assert(plants.check(assert(load(text, "=data/plants.lua", "t", {}))(), items.all()))
  end
  return cycles
end

-- Whether season s (0 .. 4) lies in window w, which may run past 4 into the
-- next year.
local function within(w, s)
  if not w then return false end
  if w[2] - w[1] >= 4 then return true end
  return (s - w[1]) % 4 <= w[2] - w[1]
end

-- A plant's own timing: its variant shifts it up to about a week either way.
local function local_season(plant, season)
  return (season - ((plant.variant or 128) / 255 - 0.5) * 0.15) % 4
end

-- What a plant bears in a season (world.sky(pos).season): leaves, flowers,
-- fruit and a word for it.
function plants.stage(plant, season)
  local cycle = plants.cycles()[plant.species] or {}
  local s = local_season(plant, season)
  local st = { leaves = within(cycle.leaves, s), flowers = within(cycle.flowers, s), fruit = within(cycle.fruit, s) }
  st.name = st.fruit and "in fruit" or st.flowers and "in flower" or st.leaves and "in leaf" or "bare"
  return st
end

function plants.new(saved)
  local state = { taken = {} }
  for id, parts in pairs(type(saved) == "table" and saved.taken or {}) do
    state.taken[tostring(id)] = {}
    for item, when in pairs(parts) do state.taken[tostring(id)][item] = when end
  end
  return state
end

-- A number 0 .. 1 from a plant, an item and a year: the same harvest gives
-- the same count.
local function roll(id, item, year)
  local h = (id % 2147483647) * 31 + year * 7919
  for k = 1, #item do h = (h * 33 + item:byte(k)) % 4294967296 end
  h = (h ~ (h >> 15)) * 2246822519 % 4294967296
  return (h ~ (h >> 13)) / 4294967296
end

local function count_of(plant, part, now)
  local size = plant.size or 0.5
  local base = part.yield[1] + (part.yield[2] - part.yield[1]) * size
  local year = math.floor(now / (plants.DAY * plants.YEAR))
  return math.max(1, math.floor(base * (0.75 + 0.5 * roll(plant.id or 0, part.item, year)) + 0.5))
end

-- The parts a plant offers now: { item, count, whole }.
function plants.available(state, plant, now, season)
  local cycle = plants.cycles()[plant.species]
  if not cycle then return {} end
  local st = plants.stage(plant, season)
  local taken = state.taken[tostring(plant.id)] or {}
  local out = {}
  for _, p in ipairs(cycle.parts) do
    local ready = p.whole or not taken[p.item] or now - taken[p.item] >= p.regrow * plants.DAY
    if ready and (not p.when or st[p.when]) then
      out[#out + 1] = { item = p.item, count = count_of(plant, p, now), whole = p.whole or false }
    end
  end
  return out
end

-- Takes a part from a plant now; returns the item and count, or nil and why.
-- A whole part cuts the plant.
function plants.take(state, plant, item, now, season)
  for _, a in ipairs(plants.available(state, plant, now, season)) do
    if a.item == item then
      if a.whole then
        if not world.cut_plant(plant.id) then return nil, "it is gone" end
        state.taken[tostring(plant.id)] = nil
      else
        local key = tostring(plant.id)
        state.taken[key] = state.taken[key] or {}
        state.taken[key][item] = now
      end
      return item, a.count
    end
  end
  return nil, "it has no " .. items.name(item) .. " now"
end

-- What to keep with the game: what was taken in the last KEEP_DAYS.
function plants.snapshot(state, now)
  local out = { taken = {} }
  for id, parts in pairs(state.taken) do
    for item, when in pairs(parts) do
      if now - when < plants.KEEP_DAYS * plants.DAY then
        out.taken[id] = out.taken[id] or {}
        out.taken[id][item] = when
      end
    end
  end
  return out
end

-- The plants within radius of pos with their stage and what they offer now.
function plants.near(state, pos, radius, now)
  local season = world.sky(pos).season
  local out = {}
  for _, p in ipairs(world.plants(pos, radius)) do
    p.stage = plants.stage(p, season)
    p.offers = plants.available(state, p, now, season)
    out[#out + 1] = p
  end
  return out
end

return plants
