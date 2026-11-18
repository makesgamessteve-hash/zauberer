-- SPDX-License-Identifier: MIT
-- What the mage carries outside the bag (concept 7.2: the child's limits): a
-- stack per item with its microbe load, up to CARRY kg. Food spoils in it as
-- game time passes, faster when warm and wet.
local items = require("life.items")
local smalllife = require("life.smalllife")
local news = require("player.news")

local pack = {}

pack.CARRY = 12 -- kg

function pack.new(saved)
  local p = { stacks = {}, news = news.new() }
  for id, st in pairs(type(saved) == "table" and saved.stacks or {}) do
    if items.get(id) and (st.count or 0) > 0 then p.stacks[id] = { count = st.count, load = st.load or 0 } end
  end
  return p
end

function pack.snapshot(p)
  local out = { stacks = {} }
  for id, st in pairs(p.stacks) do out.stacks[id] = { count = st.count, load = st.load } end
  return out
end

function pack.weight(p)
  local kg = 0
  for id, st in pairs(p.stacks) do kg = kg + items.get(id).weight * st.count end
  return kg
end

function pack.count(p, id)
  return p.stacks[id] and p.stacks[id].count or 0
end

function pack.load(p, id)
  return p.stacks[id] and p.stacks[id].load or 0
end

function pack.say(p, text)
  news.say(p.news, text)
end

-- Real time passes: old news goes.
function pack.tick(p, dt)
  news.tick(p.news, dt)
end

-- Adds up to count items with a microbe load (0 fresh); returns how many fit.
function pack.add(p, id, count, load)
  local it = assert(items.get(id), "no such item: " .. tostring(id))
  local room = math.floor((pack.CARRY - pack.weight(p)) / it.weight + 1e-9)
  local n = math.max(math.min(count, room), 0)
  if n > 0 then
    local st = p.stacks[id] or { count = 0, load = 0 }
    st.load = (st.load * st.count + (load or 0) * n) / (st.count + n)
    st.count = st.count + n
    p.stacks[id] = st
  end
  if n < count then pack.say(p, "Too heavy: " .. (count - n) .. " " .. it.name:lower() .. " left behind") end
  return n
end

-- Takes up to count items; returns how many and their load.
function pack.take(p, id, count)
  local st = p.stacks[id]
  if not st then return 0, 0 end
  local n = math.min(count, st.count)
  st.count = st.count - n
  local load = st.load
  if st.count == 0 then p.stacks[id] = nil end
  return n, load
end

-- Time passes in the pack: hours of game time at a temperature, wet or dry.
function pack.spoil(p, hours, temperature, wet)
  for id, st in pairs(p.stacks) do
    local kept = items.KEPT[items.get(id).spoils or ""]
    if kept then st.load = smalllife.spoil(st.load, hours, temperature, wet, kept) end
  end
end

-- The stacks, heaviest first: { id, name, count, load }.
function pack.list(p)
  local out = {}
  for id, st in pairs(p.stacks) do
    out[#out + 1] = { id = id, name = items.name(id), count = st.count, load = st.load,
      weight = items.get(id).weight * st.count }
  end
  table.sort(out, function(a, b) return a.weight > b.weight or (a.weight == b.weight and a.id < b.id) end)
  return out
end

function pack.hud_lines(p)
  local parts = {}
  for i, st in ipairs(pack.list(p)) do
    if i > 6 then break end
    local state = st.load >= smalllife.ROTTEN and " (rotten)" or (st.load >= smalllife.SPOILT and " (spoilt)" or "")
    parts[#parts + 1] = st.name:lower() .. " " .. st.count .. state
  end
  local lines = {}
  if #parts > 0 then
    lines[1] = string.format("Pack %.1f/%d kg: %s", pack.weight(p), pack.CARRY, table.concat(parts, ", "))
  end
  for _, text in ipairs(news.lines(p.news)) do lines[#lines + 1] = text end
  return lines
end

return pack
