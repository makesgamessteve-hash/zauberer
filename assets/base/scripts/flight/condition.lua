-- SPDX-License-Identifier: MIT
-- The carpet's condition (concept 2.1): it is cloth. Integrity (0 .. 100)
-- falls to burns (flying low over flames, or a fire under the tent) and
-- tears (skidding along the ground fast); wetness (0 .. 1) rises in rain
-- and on water and dries off with time, warmth and a fire. A soaked carpet
-- is heavier and slower, a torn one slower still, and at zero it can no
-- longer fly: it sinks to the ground and must be patched there. A field
-- patch takes thread (fibres, hide) and life or void mana and mends up to
-- 70; the workshop mends fully. Upgrades are woven into its slots.
local items = require("life.items")
local news = require("player.news")
local pack = require("player.pack")

local condition = {}

condition.SLOTS = 3          -- upgrade slots at the start
condition.BURN = 8           -- integrity a second over flames
condition.TEAR = 4           -- integrity a second skidding along the ground fast
condition.SKID = 30          -- m/s along the ground past which it tears
condition.LOW = 4            -- metres above the ground within which flames reach it
condition.RAIN_SOAK = 0.08   -- wetness an hour per mm/h of rain
condition.WATER_SOAK = 0.1   -- wetness a second on the water
condition.DRY_HOURS = 2      -- hours to dry from soaked in mild air
condition.FIELD_MAX = 70     -- integrity a field patch mends up to
condition.PATCH = 20         -- integrity a patch mends
condition.PATCH_THREAD = 2   -- thread a patch takes
condition.PATCH_MANA = 2     -- life or void mana a patch takes
condition.AIRWORTHY = 10     -- integrity needed to take off again

local registry
function condition.upgrades()
  if not registry then
    local text = assert(vfs.read("data/carpet_upgrades.lua"))
    registry = {}
    for _, u in ipairs(assert(load(text, "=data/carpet_upgrades.lua", "t", {}))()) do registry[u.id] = u end
  end
  return registry
end

function condition.new(saved)
  saved = type(saved) == "table" and saved or {}
  local c = { integrity = math.max(math.min(tonumber(saved.integrity) or 100, 100), 0),
    wetness = math.max(math.min(tonumber(saved.wetness) or 0, 1), 0),
    slots = math.max(math.floor(tonumber(saved.slots) or condition.SLOTS), 0), upgrades = {}, news = news.new(),
    grounded_out = saved.grounded_out and true or false }
  for _, id in ipairs(type(saved.upgrades) == "table" and saved.upgrades or {}) do
    if condition.upgrades()[id] and #c.upgrades < c.slots then c.upgrades[#c.upgrades + 1] = id end
  end
  return c
end

function condition.snapshot(c)
  local ups = {}
  for i, id in ipairs(c.upgrades) do ups[i] = id end
  return { integrity = c.integrity, wetness = c.wetness, slots = c.slots, upgrades = ups,
    grounded_out = c.grounded_out or nil }
end

-- Whether an installed upgrade gives an effect (its value, or nil).
function condition.has(c, effect)
  local value
  for _, id in ipairs(c.upgrades) do
    local u = condition.upgrades()[id]
    if u and u.effect[effect] ~= nil then value = u.effect[effect] end
  end
  return value
end

-- Weaves an upgrade into a free slot; true, or nil and why.
function condition.install(c, id)
  local u = condition.upgrades()[id]
  if not u then return nil, "no such upgrade" end
  for _, have in ipairs(c.upgrades) do
    if have == id then return nil, "already woven in" end
  end
  if #c.upgrades >= c.slots then return nil, "no free slot" end
  c.upgrades[#c.upgrades + 1] = id
  news.say(c.news, u.name .. " woven into the carpet")
  return true
end

local function hurt(c, amount)
  local before = c.integrity
  c.integrity = math.max(c.integrity - amount, 0)
  if before > 0 and c.integrity == 0 then
    c.grounded_out = true
    news.say(c.news, "The carpet is torn to the weft: it sinks; patch it on the ground")
  elseif before >= 30 and c.integrity < 30 then
    news.say(c.news, "The carpet is badly torn")
  end
end

-- Time passes. dt: real seconds; hours: game hours. ctx: { exposed (in
-- flight or laid out, not worn or in the bag), height (m above the ground),
-- speed (m/s), grounded, on_water, burning (flames under it), rain (mm/h),
-- air (deg C), fire (a fire close by) }.
function condition.tick(c, dt, hours, ctx)
  news.tick(c.news, dt)
  if ctx.exposed then
    if ctx.burning and (ctx.height or 0) <= condition.LOW then
      hurt(c, condition.BURN * dt * (condition.has(c, "fire_ward") or 1))
    end
    if ctx.grounded and (ctx.speed or 0) > condition.SKID then hurt(c, condition.TEAR * dt) end
    if ctx.on_water then c.wetness = math.min(c.wetness + condition.WATER_SOAK * dt, 1) end
  end
  local raining = (ctx.rain or 0) > 0.2 and ctx.exposed and not condition.has(c, "weatherproof")
  if raining then
    c.wetness = math.min(c.wetness + condition.RAIN_SOAK * ctx.rain * hours, 1)
  elseif not ctx.on_water then
    local air = ctx.air or 15
    local rate = (air > 0 and (1 + air / 20) or 0.3) * (ctx.fire and 4 or 1)
    c.wetness = math.max(c.wetness - hours / condition.DRY_HOURS * rate, 0)
  end
end

-- What the condition allows in flight: { speed, climb (factors), fly }.
function condition.limits(c)
  local wet = c.wetness
  local torn = c.integrity < 30 and (0.6 + 0.4 * c.integrity / 30) or 1
  return { speed = (1 - 0.3 * wet) * torn * (condition.has(c, "speed") or 1), climb = (1 - 0.5 * wet) * torn,
    fly = not c.grounded_out }
end

-- A patch in the field: thread from the pack and life or void mana; the
-- carpet mends by PATCH up to FIELD_MAX. Returns the integrity, or nil and
-- why.
function condition.patch(c, p, mage)
  if c.integrity >= condition.FIELD_MAX then return nil, "a field patch does no better" end
  local thread = 0
  for id, st in pairs(p.stacks) do thread = thread + (items.get(id).thread or 0) * st.count end
  if thread < condition.PATCH_THREAD then return nil, "no thread: fibres or hide" end
  local colour = (mage.mana.life or 0) >= condition.PATCH_MANA and "life"
    or ((mage.mana.void or 0) >= condition.PATCH_MANA and "void" or nil)
  if not colour then return nil, "too little life or void mana" end
  mage.mana[colour] = mage.mana[colour] - condition.PATCH_MANA
  local need = condition.PATCH_THREAD
  local ids = {}
  for id in pairs(p.stacks) do
    if items.get(id).thread then ids[#ids + 1] = id end
  end
  table.sort(ids)
  for _, id in ipairs(ids) do
    local per = items.get(id).thread
    while need > 0 and pack.count(p, id) > 0 do
      pack.take(p, id, 1)
      need = need - per
    end
  end
  c.integrity = math.min(c.integrity + condition.PATCH, condition.FIELD_MAX)
  if c.integrity >= condition.AIRWORTHY then c.grounded_out = false end
  news.say(c.news, string.format("Patched: the carpet is %d%% whole", math.floor(c.integrity + 0.5)))
  return c.integrity
end

-- A full repair (the workshop): up to quality (default 100).
function condition.repair(c, quality)
  c.integrity = math.max(c.integrity, math.min(quality or 100, 100))
  if c.integrity >= condition.AIRWORTHY then c.grounded_out = false end
end

function condition.say(c, text) news.say(c.news, text) end

function condition.hud_lines(c)
  local lines = {}
  if c.integrity < 99.5 or c.wetness > 0.1 then
    local wet = c.wetness > 0.7 and ", soaked" or (c.wetness > 0.3 and ", wet" or (c.wetness > 0.1 and ", damp" or ""))
    lines[1] = string.format("Carpet %d%% whole%s%s", math.floor(c.integrity + 0.5), wet,
      c.grounded_out and " - grounded: patch it (M)" or "")
  end
  for _, text in ipairs(news.lines(c.news)) do lines[#lines + 1] = text end
  return lines
end

return condition
