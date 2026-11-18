-- SPDX-License-Identifier: MIT
-- The item registry (data/items.lua): what plants, creatures and the land
-- give, and what is made of them.
local items = {}

items.KINDS = { food = true, herb = true, material = true, water = true }

local registry -- id -> item, read on first use

-- Checks a list of items; returns them by id, or nil and what is wrong.
function items.check(list)
  if type(list) ~= "table" or #list == 0 then return nil, "no items" end
  local by_id = {}
  for i, it in ipairs(list) do
    local where = "item " .. i
    if type(it.id) ~= "string" or it.id == "" then return nil, where .. ": no id" end
    where = "item " .. it.id
    if by_id[it.id] then return nil, where .. ": twice" end
    if type(it.name) ~= "string" then return nil, where .. ": no name" end
    if not items.KINDS[it.kind] then return nil, where .. ": unknown kind " .. tostring(it.kind) end
    if type(it.weight) ~= "number" or it.weight <= 0 or it.weight > 1000 then
      return nil, where .. ": weight must be 0 .. 1000 kg"
    end
    by_id[it.id] = it
  end
  return by_id
end

-- All items by id.
function items.all()
  if not registry then
    local text = assert(vfs.read("data/items.lua"))
    registry = assert(items.check(assert(load(text, "=data/items.lua", "t", {}))()))
  end
  return registry
end

function items.get(id) return items.all()[id] end

function items.name(id)
  local it = items.get(id)
  return it and it.name or id
end

return items
