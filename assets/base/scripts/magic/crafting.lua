-- SPDX-License-Identifier: MIT
-- Crafting at the workbench (concept 3.4): cord from any thread, a horn
-- lantern, a staff, a wand; and the carpet mended fully (the workshop's
-- repair). The recipes are data (data/crafts.lua); "thread" is counted in
-- units from fibres, hides and cord.
local items = require("life.items")
local pack = require("player.pack")

local crafting = {}

local registry
function crafting.crafts()
  if not registry then
    local text = assert(vfs.read("data/crafts.lua"))
    registry = assert(load(text, "=data/crafts.lua", "t", {}))()
    for _, c in ipairs(registry) do
      for id in pairs(c.needs) do assert(id == "thread" or items.get(id), "craft " .. c.id .. ": no item " .. id) end
      for id in pairs(c.makes or {}) do assert(items.get(id), "craft " .. c.id .. ": no item " .. id) end
    end
  end
  return registry
end

-- Units of thread in the pack.
function crafting.thread(p)
  local n = 0
  for id, st in pairs(p.stacks) do n = n + (items.get(id).thread or 0) * st.count end
  return n
end

local function have(p, id)
  return id == "thread" and crafting.thread(p) or pack.count(p, id)
end

-- What a craft lacks: { item = missing }.
function crafting.lacks(c, p)
  local out = {}
  for id, n in pairs(c.needs) do
    if have(p, id) < n then out[id] = n - have(p, id) end
  end
  return out
end

local function take_thread(p, n)
  local ids = {}
  for id in pairs(p.stacks) do
    if items.get(id).thread and id ~= "cord" then ids[#ids + 1] = id end
  end
  table.sort(ids, function(a, b)
    local ta, tb = items.get(a).thread, items.get(b).thread
    return ta < tb or (ta == tb and a < b)
  end)
  ids[#ids + 1] = pack.count(p, "cord") > 0 and "cord" or nil
  for _, id in ipairs(ids) do
    while n > 0 and pack.count(p, id) > 0 do
      pack.take(p, id, 1)
      n = n - items.get(id).thread
    end
  end
end

-- Makes a craft from the pack; repairs go to the carpet's condition (cond,
-- with the condition module cmod). Returns { item = count } made (empty for
-- a repair) and the hours, or nil and why.
function crafting.craft(c, p, cond, cmod)
  local lack = crafting.lacks(c, p)
  if next(lack) then
    local parts = {}
    for id, n in pairs(lack) do
      parts[#parts + 1] = (id == "thread" and "thread" or items.name(id):lower()) .. " " .. n
    end
    table.sort(parts)
    return nil, "not enough: " .. table.concat(parts, ", ")
  end
  if c.repair and cond and cond.integrity >= 99.5 then return nil, "the carpet is whole" end
  for id, n in pairs(c.needs) do
    if id == "thread" then take_thread(p, n) else pack.take(p, id, n) end
  end
  local made = {}
  for id, n in pairs(c.makes or {}) do made[id] = pack.add(p, id, n, 0) end
  if c.repair and cond then cmod.repair(cond) end
  return made, c.hours
end

function crafting.line(c, p)
  local parts = {}
  for id, n in pairs(c.needs) do
    parts[#parts + 1] = string.format("%s %d/%d", id == "thread" and "thread" or items.name(id):lower(), have(p, id), n)
  end
  table.sort(parts)
  return string.format("Work: %s (%s; %.1f h) - K to make, . for the next", c.name, table.concat(parts, ", "), c.hours)
end

return crafting
