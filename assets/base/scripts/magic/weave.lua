-- SPDX-License-Identifier: MIT
-- The weave grammar (concept 3.3): a weave is a verb, one or two colours, a
-- form, modifiers and power. This module checks and names weaves, prices them
-- (base x form x modifiers x distance x place x (1 - efficiency)), judges
-- their control and misfire chance, and resolves a cast into a plan of
-- simulation effects. It touches no simulation itself (magic.effects does).
local weave = {}

local data -- loaded on first use from data/weaves.lua
local INDEX = {} -- colour -> place on the wheel (1 .. 8)

function weave.data()
  if not data then
    local text = assert(vfs.read("data/weaves.lua"))
    data = assert(load(text, "=data/weaves.lua", "t", {}))()
    for i, c in ipairs(data.colors) do
      INDEX[c.id] = i
    end
  end
  return data
end

-- "same", "neighbour" (adjacent on the wheel), "opposite" (four apart) or "other".
function weave.relation(a, b)
  weave.data()
  local d = math.abs(INDEX[a] - INDEX[b])
  d = math.min(d, 8 - d)
  return d == 0 and "same" or d == 1 and "neighbour" or d == 4 and "opposite" or "other"
end

-- The colours of a weave in wheel order (a key does not depend on how they were picked).
local function sorted_colors(colors)
  local out = {}
  for _, c in ipairs(colors) do out[#out + 1] = c end
  table.sort(out, function(a, b) return INDEX[a] < INDEX[b] end)
  return out
end

-- Checks a weave description; returns true or false and why.
function weave.validate(w)
  local d = weave.data()
  if type(w) ~= "table" then return false, "a weave must be a table" end
  if not d.verbs[w.verb or ""] then return false, "no such verb: " .. tostring(w.verb) end
  if type(w.colors) ~= "table" or #w.colors < 1 or #w.colors > 2 then
    return false, "a weave has one or two colours"
  end
  for _, c in ipairs(w.colors) do
    if not INDEX[c] then return false, "no such colour: " .. tostring(c) end
  end
  if #w.colors == 2 and w.colors[1] == w.colors[2] then return false, "two different colours" end
  if not d.forms[w.form or ""] then return false, "no such form: " .. tostring(w.form) end
  for _, m in ipairs(w.modifiers or {}) do
    if not d.modifiers[m] then return false, "no such modifier: " .. tostring(m) end
  end
  if type(w.power) ~= "number" or w.power <= 0 or w.power > 1000 then
    return false, "power must be more than 0 and at most 1000 mana"
  end
  return true
end

-- A key naming the combination (not the power): "move:fire+storm:line".
function weave.key(w)
  weave.data()
  return w.verb .. ":" .. table.concat(sorted_colors(w.colors), "+") .. ":" .. w.form
end

-- The weave's name in the journal: a known name, or one made from its parts.
function weave.name(w)
  local d, key = weave.data(), weave.key(w)
  for _, n in ipairs(d.named) do
    if weave.key({ verb = n.verb, colors = n.colors, form = n.form }) == key then
      return n.name, true
    end
  end
  local cs = {}
  for _, c in ipairs(sorted_colors(w.colors)) do cs[#cs + 1] = d.colors[INDEX[c]].name end
  return w.verb:sub(1, 1):upper() .. w.verb:sub(2) .. " " .. table.concat(cs, " and ") .. " (" .. w.form .. ")", false
end

-- What the spoken weave sounds like in the Old Tongue: "vash atash-vad dur".
function weave.words(w)
  local d = weave.data()
  local cs = {}
  for _, c in ipairs(sorted_colors(w.colors)) do cs[#cs + 1] = d.colors[INDEX[c]].word:lower() end
  local out = { d.verbs[w.verb].word, table.concat(cs, "-") }
  for _, m in ipairs(w.modifiers or {}) do out[#out + 1] = d.modifiers[m].word end
  return table.concat(out, " ")
end

-- How far, how wide and how long a weave reaches with its modifiers.
function weave.extent(w)
  local d = weave.data()
  local f = d.forms[w.form]
  local reach, radius, duration, count, delay = f.reach, f.radius * (math.max(w.power, 1) / 10) ^ 0.25, 1, 1, 0
  for _, m in ipairs(w.modifiers or {}) do
    local mod = d.modifiers[m]
    reach = reach * (mod.reach or 1)
    radius = radius * (mod.radius or 1)
    duration = duration * (mod.duration or 1)
    count = count * (mod.count or 1)
    delay = delay + (mod.delay or 0)
  end
  return { reach = reach, radius = math.min(radius, 64), duration = duration, count = count, delay = delay }
end

-- Matter from nothing costs much more (concept 3.3): conjuring stone, and
-- water where the air holds little.
local FROM_NOTHING = { earth = 4, water = 2 }

-- The mana a weave costs per colour and in all.
-- ctx: distance (metres to the target), place (stored mana per colour where it
-- is cast), efficiency (0 .. 0.6 from attunement and familiarity), material
-- (true when matter is supplied).
function weave.cost(w, ctx)
  local d = weave.data()
  ctx = ctx or {}
  local k = d.verbs[w.verb].base * d.forms[w.form].factor
  for _, m in ipairs(w.modifiers or {}) do k = k * d.modifiers[m].cost end
  k = k * (1 + (ctx.distance or 0) / 100)
  k = k * (1 - math.min(math.max(ctx.efficiency or 0, 0), 0.6))
  local per, total = {}, 0
  local share = #w.colors == 2 and { 0.6, 0.4 } or { 1 }
  for i, c in ipairs(w.colors) do
    local stored = ctx.place and ctx.place[c] or 40
    local place = math.min(math.max(2 - stored / 40, 0.5), 3) -- rich places are cheap, thin ones dear
    local extra = (w.verb == "create" and not ctx.material and FROM_NOTHING[c]) or 1
    per[c] = w.power * share[i] * k * place * extra
    total = total + per[c]
  end
  return { per = per, total = total }
end

-- Familiarity tiers by casts (concept 3.3).
weave.TIERS = { { name = "Unfamiliar", casts = 0, bonus = 0 }, { name = "Practised", casts = 10, bonus = 0.1 },
  { name = "Known", casts = 40, bonus = 0.2 }, { name = "Mastered", casts = 120, bonus = 0.3 } }

function weave.tier(casts)
  local t = weave.TIERS[1]
  for _, x in ipairs(weave.TIERS) do
    if (casts or 0) >= x.casts then t = x end
  end
  return t
end

-- Control 0 .. 1 from the gesture's accuracy and familiarity.
function weave.control(accuracy, casts)
  return math.min(1, 0.65 * (accuracy or 0) + 0.25 + weave.tier(casts).bonus * 0.35)
end

-- The chance of a misfire: poor control, opposing colours, overcharging and
-- weaves never cast before raise it; harmonious colours lower it.
function weave.misfire_chance(w, control, overcharge, casts)
  local p = (1 - control) * 0.5
  if #w.colors == 2 then
    local rel = weave.relation(w.colors[1], w.colors[2])
    p = p + (rel == "opposite" and 0.2 or 0) - (rel == "neighbour" and 0.05 or 0)
  end
  p = p + math.max(overcharge or 0, 0) * 0.5
  if (casts or 0) == 0 then p = p + 0.1 end
  return math.min(math.max(p, 0.02), 0.9)
end

-- A small deterministic random stream (the same on every machine).
function weave.rng(seed)
  local s = seed | 1
  return function()
    s = s * 6364136223846793005 + 1442695040888963407
    return ((s >> 11) & 0x1FFFFFFFFFFFFF) / 9007199254740992.0
  end
end

local MISFIRES = { "backfire", "stray", "weak", "late", "overshoot" }

-- Resolves a cast into a plan: the weave, its extent, the power that reaches
-- the effect, and a misfire if the roll says so.
-- ctx: control, overcharge, casts (familiarity), seed.
function weave.resolve(w, ctx)
  local ok, err = weave.validate(w)
  if not ok then return nil, err end
  local rnd = weave.rng(ctx.seed or 1)
  local plan = { weave = w, extent = weave.extent(w), power = w.power, delay = 0, offset = { 0, 0 } }
  plan.chance = weave.misfire_chance(w, ctx.control or 1, ctx.overcharge, ctx.casts)
  plan.delay = plan.extent.delay
  if rnd() < plan.chance then
    local kind = MISFIRES[1 + math.floor(rnd() * #MISFIRES)]
    plan.misfire = kind
    if kind == "weak" then
      plan.power = w.power * 0.4
    elseif kind == "overshoot" then
      plan.power = w.power * 1.6
    elseif kind == "late" then
      plan.delay = plan.delay + 1 + 2 * rnd()
    elseif kind == "stray" then
      local a, r = rnd() * 2 * math.pi, 3 + 7 * rnd()
      plan.offset = { math.cos(a) * r, math.sin(a) * r }
    end -- "backfire": the effect goes off at the caster's hand
  end
  return plan
end

return weave
