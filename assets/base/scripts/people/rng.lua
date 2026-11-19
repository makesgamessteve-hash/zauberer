-- SPDX-License-Identifier: MIT
-- Counter-based randomness for world generation (implementation plan 1.5):
-- every value is a hash of a seed and keys, so the same world gets the same
-- peoples, names and history on every platform, whatever order things are
-- asked for in. Streams (rng.new) draw values one after another from a seed.
local rng = {}

-- splitmix64 constants (0xbf58476d1ce4e5b9, 0x94d049bb133111eb)
local M1, M2 = -4658895280553007687, -7723592293110705685
local GOLDEN = -7046029254386353131 -- 0x9e3779b97f4a7c15

local function mix(z)
  z = (z ~ (z >> 30)) * M1
  z = (z ~ (z >> 27)) * M2
  return z ~ (z >> 31)
end

-- A 64-bit hash of a seed and any keys (integers, numbers or strings).
function rng.hash(seed, ...)
  local h = mix(math.tointeger(seed) or 0)
  for i = 1, select("#", ...) do
    local k = select(i, ...)
    if type(k) == "string" then
      for c = 1, #k do h = mix(h ~ (k:byte(c) + c * 131)) end
      h = mix(h + #k)
    else
      local n = math.tointeger(k) or math.floor(k * 1024)
      h = mix(h + GOLDEN + n)
    end
  end
  return h
end

-- A number in [0, 1) from a hash.
function rng.unit(h)
  return ((h >> 11) & 0x1fffffffffffff) / 9007199254740992.0
end

-- A number in [0, 1) for a seed and keys.
function rng.float(seed, ...)
  return rng.unit(rng.hash(seed, ...))
end

local Stream = {}
Stream.__index = Stream

function rng.new(seed, ...)
  return setmetatable({ state = rng.hash(seed, ...), n = 0 }, Stream)
end

function Stream:next()
  self.n = self.n + 1
  return mix(self.state + self.n * GOLDEN)
end

function Stream:float()
  return rng.unit(self:next())
end

-- An integer in [lo, hi].
function Stream:int(lo, hi)
  return lo + math.floor(self:float() * (hi - lo + 1))
end

function Stream:chance(p)
  return self:float() < p
end

function Stream:pick(list)
  return list[self:int(1, #list)]
end

-- Picks from { {value, weight}, ... } (or a map value -> weight, in sorted key order).
function Stream:weighted(choices)
  local list = choices
  if choices[1] == nil then
    list = {}
    local keys = {}
    for k in pairs(choices) do keys[#keys + 1] = k end
    table.sort(keys)
    for _, k in ipairs(keys) do list[#list + 1] = { k, choices[k] } end
  end
  local total = 0
  for _, c in ipairs(list) do total = total + math.max(0, c[2]) end
  if total <= 0 then return list[1] and list[1][1] end
  local x = self:float() * total
  for _, c in ipairs(list) do
    x = x - math.max(0, c[2])
    if x < 0 then return c[1] end
  end
  return list[#list][1]
end

function Stream:shuffle(list)
  for i = #list, 2, -1 do
    local j = self:int(1, i)
    list[i], list[j] = list[j], list[i]
  end
  return list
end

return rng
