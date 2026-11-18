-- SPDX-License-Identifier: MIT
-- Farrokh's endless bag (concept 2.4): a small satchel holding a folded
-- space. Goods take folds, not weight: a fold holds a stack of one item, and
-- the number of folds grows with upgrades. The bag and the tent's vault are
-- one space: what is on the vault's shelves can be reached through the bag,
-- for a little Void mana and a few seconds each stack; inside the vault,
-- goods move freely. Time in the bag passes at a tenth, so food keeps longer
-- (never for ever). Living creatures are refused.
local items = require("life.items")
local smalllife = require("life.smalllife")
local news = require("player.news")
local pack = require("player.pack")

local bag = {}

local NEED = { food = 1, water = 2, material = 3, herb = 4 }

bag.FOLDS = 12           -- folds at the start
bag.STACK = 50           -- items of one kind a fold holds
bag.SLOW = 0.1           -- time in the bag (and the vault) passes at this rate
bag.REACH_MANA = 1       -- void mana to bring a stack from the vault through the bag
bag.REACH_SECONDS = 3    -- real seconds the bag is busy reaching

local function copy_stacks(saved)
  local out = {}
  for id, st in pairs(type(saved) == "table" and saved or {}) do
    if items.get(id) and type(st) == "table" and (tonumber(st.count) or 0) > 0 then
      out[id] = { count = math.floor(st.count), load = tonumber(st.load) or 0 }
    end
  end
  return out
end

function bag.new(saved)
  saved = type(saved) == "table" and saved or {}
  return { folds = math.max(math.floor(tonumber(saved.folds) or bag.FOLDS), 1), stacks = copy_stacks(saved.stacks),
    vault = copy_stacks(saved.vault), busy = 0, news = news.new() }
end

function bag.snapshot(b)
  local function out(stacks)
    local t = {}
    for id, st in pairs(stacks) do t[id] = { count = st.count, load = st.load } end
    return t
  end
  return { folds = b.folds, stacks = out(b.stacks), vault = out(b.vault) }
end

-- Folds a count of an item takes.
local function folds_of(count) return math.ceil(count / bag.STACK) end

function bag.folds_used(b)
  local n = 0
  for _, st in pairs(b.stacks) do n = n + folds_of(st.count) end
  return n
end

function bag.count(b, id) return b.stacks[id] and b.stacks[id].count or 0 end
function bag.in_vault(b, id) return b.vault[id] and b.vault[id].count or 0 end

local function add(stacks, id, n, load)
  local st = stacks[id] or { count = 0, load = 0 }
  st.load = (st.load * st.count + (load or 0) * n) / (st.count + n)
  st.count = st.count + n
  stacks[id] = st
end

local function remove(stacks, id, n)
  local st = stacks[id]
  if not st then return 0, 0 end
  n = math.min(n, st.count)
  st.count = st.count - n
  local load = st.load
  if st.count == 0 then stacks[id] = nil end
  return n, load
end

-- Puts up to count items in the bag's folds; returns how many went in.
function bag.put(b, id, count, load)
  assert(items.get(id), "no such item: " .. tostring(id))
  local have = bag.count(b, id)
  local free_folds = b.folds - bag.folds_used(b)
  local room = free_folds * bag.STACK + (folds_of(have) * bag.STACK - have) -- (room left in its own last fold)
  local n = math.max(math.min(count, room), 0)
  if n > 0 then add(b.stacks, id, n, load) end
  if n < count then news.say(b.news, "The bag is full: no fold for " .. items.name(id):lower()) end
  return n
end

-- A living creature is refused (with a shimmer at the bag's mouth).
function bag.put_creature(b, creature)
  if creature and creature.alive then
    news.say(b.news, "The bag will not take the living")
    b.refused = (b.refused or 0) + 1
    return false
  end
  news.say(b.news, "Butcher it first")
  return false
end

-- Takes up to count of an item from the folds; what is short is reached
-- from the vault through the bag, a stack at a time, for void mana from the
-- mage (when given) while the bag is not busy. Returns how many, their
-- microbe load, and whether the vault was reached.
function bag.take(b, id, count, mage)
  local n, load = remove(b.stacks, id, count)
  local reached = false
  if n < count and bag.in_vault(b, id) > 0 and mage then
    if b.busy > 0 then
      news.say(b.news, "The bag is still reaching")
    elseif (mage.mana.void or 0) < bag.REACH_MANA then
      news.say(b.news, "Too little void mana to reach the vault")
    else
      mage.mana.void = mage.mana.void - bag.REACH_MANA
      b.busy = bag.REACH_SECONDS
      local m, vload = remove(b.vault, id, math.min(count - n, bag.STACK))
      load = n + m > 0 and (load * n + vload * m) / (n + m) or 0
      n = n + m
      reached = true
      news.say(b.news, "You reach into the vault: " .. items.name(id):lower() .. " " .. m)
    end
  end
  return n, load, reached
end

-- In the vault: goods move between the folds and the shelves freely.
function bag.to_vault(b, id, count)
  local n, load = remove(b.stacks, id, count)
  if n > 0 then add(b.vault, id, n, load) end
  return n
end

function bag.from_vault(b, id, count)
  local st = b.vault[id]
  if not st then return 0 end
  local n = bag.put(b, id, math.min(count, st.count), st.load)
  remove(b.vault, id, n)
  return n
end

-- In the vault: everything carried (the pack and the folds) onto the shelves;
-- returns { id = count }.
function bag.shelve_all(b, p)
  local moved = {}
  for _, stacks in ipairs({ p.stacks, b.stacks }) do
    local ids = {}
    for id in pairs(stacks) do ids[#ids + 1] = id end
    for _, id in ipairs(ids) do
      local n, load = remove(stacks, id, stacks[id].count)
      add(b.vault, id, n, load)
      moved[id] = (moved[id] or 0) + n
    end
  end
  return moved
end

-- In the vault: the road's needs off the shelves into the pack, as far as
-- it carries (food and drink first), freely; returns { id = count }.
function bag.take_from_shelves(b, p)
  local list = {}
  for id, st in pairs(b.vault) do
    local it = items.get(id)
    local rank = NEED[it.kind] or 5
    if it.kind == "material" and not it.fuel then rank = 5 end
    list[#list + 1] = { id = id, rank = rank, load = st.load }
  end
  table.sort(list, function(x, y)
    if x.rank ~= y.rank then return x.rank < y.rank end
    if x.load ~= y.load then return x.load < y.load end
    return x.id < y.id
  end)
  local moved = {}
  for _, e in ipairs(list) do
    local room = math.max(math.floor((pack.CARRY - pack.weight(p)) / items.get(e.id).weight + 1e-9), 0)
    local n = math.min(b.vault[e.id].count, room)
    if n > 0 and e.rank < 5 then
      local _, load = remove(b.vault, e.id, n)
      pack.add(p, e.id, n, load)
      moved[e.id] = n
    end
  end
  return moved
end

-- Everything in the pack goes into the bag, as far as the folds go; returns
-- { id = count } stowed.
function bag.stow(b, p)
  local moved = {}
  local ids = {}
  for id in pairs(p.stacks) do ids[#ids + 1] = id end
  table.sort(ids)
  for _, id in ipairs(ids) do
    local st = p.stacks[id]
    local n = bag.put(b, id, st.count, st.load)
    if n > 0 then
      pack.take(p, id, n)
      moved[id] = n
    end
  end
  return moved
end

-- What is needed on the road comes out into the pack, as far as it can carry:
-- food (the best kept first), drink, firewood, herbs, the rest; with a mage
-- to pay the void mana, food or drink the folds lack is reached from the
-- vault. Returns { id = count }.
function bag.unstow(b, p, mage)
  local list = {}
  for id, st in pairs(b.stacks) do
    local it = items.get(id)
    local rank = NEED[it.kind] or 5
    if it.kind == "material" and not it.fuel then rank = 5 end
    list[#list + 1] = { id = id, rank = rank, load = st.load }
  end
  table.sort(list, function(x, y)
    if x.rank ~= y.rank then return x.rank < y.rank end
    if x.load ~= y.load then return x.load < y.load end
    return x.id < y.id
  end)
  local moved = {}
  local function out(id, n, load)
    pack.add(p, id, n, load)
    moved[id] = (moved[id] or 0) + n
  end
  local function room_for(id)
    return math.max(math.floor((pack.CARRY - pack.weight(p)) / items.get(id).weight + 1e-9), 0)
  end
  local function from_vault(kind) -- food or drink the pack lacks, reached from the vault
    for id in pairs(p.stacks) do
      if items.get(id).kind == kind then return end
    end
    local best
    for id, st in pairs(b.vault) do
      if items.get(id).kind == kind and st.load < smalllife.SPOILT and (not best or st.load < b.vault[best].load) then
        best = id
      end
    end
    if mage and best then
      local n, load = bag.take(b, best, math.min(room_for(best), bag.STACK), mage)
      if n > 0 then out(best, n, load) end
    end
  end
  local reached = false
  for _, e in ipairs(list) do
    if e.rank > 2 and not reached then
      from_vault("food")
      from_vault("water")
      reached = true
    end
    local n = math.min(b.stacks[e.id].count, room_for(e.id))
    if n > 0 then out(e.id, n, select(2, remove(b.stacks, e.id, n))) end
  end
  if not reached then
    from_vault("food")
    from_vault("water")
  end
  return moved
end

-- Time passes: real seconds (the bag's reach), and game hours at a
-- temperature (spoiling, slowed).
function bag.tick(b, dt)
  b.busy = math.max(b.busy - dt, 0)
  news.tick(b.news, dt)
end

function bag.spoil(b, hours, temperature)
  for _, stacks in ipairs({ b.stacks, b.vault }) do
    for id, st in pairs(stacks) do
      local kept = items.KEPT[items.get(id).spoils or ""]
      if kept then st.load = smalllife.spoil(st.load, hours * bag.SLOW, temperature, false, kept) end
    end
  end
end

function bag.say(b, text) news.say(b.news, text) end

function bag.hud_lines(b)
  local lines = {}
  local parts = {}
  local ids = {}
  for id in pairs(b.stacks) do ids[#ids + 1] = id end
  table.sort(ids)
  for i, id in ipairs(ids) do
    if i > 5 then
      parts[#parts + 1] = "..."
      break
    end
    parts[#parts + 1] = items.name(id):lower() .. " " .. b.stacks[id].count
  end
  if #parts > 0 then
    lines[1] = string.format("Bag %d/%d folds: %s", bag.folds_used(b), b.folds, table.concat(parts, ", "))
  end
  for _, text in ipairs(news.lines(b.news)) do lines[#lines + 1] = text end
  return lines
end

return bag
