-- SPDX-License-Identifier: MIT
-- Ritual magic (concept 3.4): the mage lays a circle on the ground around
-- them with ingredients at its points, at the right time (dawn and dusk,
-- night or day), and channels for some seconds, still and unhurt. The
-- ritual draws its mana from the land around first (heavy drawing empties
-- cells and blights them) and the rest from the mage. Moving away or being
-- hurt breaks it: the drawn mana lashes back. Done, it acts on the region
-- for hours: rain is called, a camp is warded, a room of the pocket home
-- unfolds.
local items = require("life.items")
local pack = require("player.pack")

local ritual = {}

ritual.RADIUS = 4      -- metres of the circle
ritual.DRAW = 64       -- metres around within which the land gives its mana
ritual.BACKLASH = 0.5  -- harm per mana drawn when a ritual breaks

local registry
function ritual.rituals()
  if not registry then
    local text = assert(vfs.read("data/rituals.lua"))
    registry = assert(load(text, "=data/rituals.lua", "t", {}))()
  end
  return registry
end

function ritual.get(id)
  for _, r in ipairs(ritual.rituals()) do
    if r.id == id then return r end
  end
end

function ritual.new(saved)
  saved = type(saved) == "table" and saved or {}
  local s = { chosen = 1, effects = {} }
  for kind, e in pairs(type(saved.effects) == "table" and saved.effects or {}) do
    if type(e) == "table" and tonumber(e.until_time) then s.effects[kind] = e end
  end
  return s
end

function ritual.snapshot(s)
  return { effects = s.effects }
end

-- Whether it is the time for a ritual at daylight (0 night .. 1 day).
function ritual.timely(r, daylight)
  if r.timing == "twilight" then return daylight > 0.02 and daylight < 0.5 end
  if r.timing == "night" then return daylight <= 0.02 end
  if r.timing == "day" then return daylight >= 0.5 end
  return true
end

-- Lays the circle and begins. ctx: { pos, pack, daylight, near_tent, on_foot,
-- factor (mastery: scales the mana) }. Returns true, or nil and why.
function ritual.begin(s, r, ctx)
  if s.active then return nil, "a ritual is already under way" end
  if not ctx.on_foot then return nil, "a circle is laid on foot" end
  if not ritual.timely(r, ctx.daylight) then return nil, "not the time: " .. r.timing end
  if r.effect == "room" and not ctx.near_tent then return nil, "by the pitched tent" end
  for id, n in pairs(r.needs or {}) do
    if pack.count(ctx.pack, id) < n then return nil, "not enough " .. items.name(id):lower() end
  end
  for id, n in pairs(r.needs or {}) do pack.take(ctx.pack, id, n) end
  local due = {}
  for colour, n in pairs(r.mana) do due[colour] = n * (ctx.factor or 1) end
  s.active = { id = r.id, pos = ctx.pos, elapsed = 0, due = due, drawn = 0, from_land = 0 }
  return true
end

-- Channelling: dt seconds; ctx: { pos, mage, hurt, now }. Returns nil while
-- under way, or what happened: { done = ritual, from_land, blighted } or
-- { broken = ritual, backlash = harm, why }.
function ritual.tick(s, dt, ctx)
  local a = s.active
  if not a then return nil end
  local r = ritual.get(a.id)
  if ctx.hurt or world.distance(ctx.pos, a.pos) > ritual.RADIUS then
    s.active = nil
    return { broken = r, backlash = a.drawn * ritual.BACKLASH, why = ctx.hurt and "hurt" or "the circle left" }
  end
  local step = math.min(dt, r.channel - a.elapsed)
  for colour in pairs(r.mana) do
    local want = a.due[colour] * step / r.channel
    local got = mana.draw(a.pos, colour, want, ritual.DRAW)
    a.from_land = a.from_land + got
    local rest = math.min(want - got, ctx.mage.mana[colour] or 0)
    ctx.mage.mana[colour] = (ctx.mage.mana[colour] or 0) - rest
    a.drawn = a.drawn + got + rest
    if got + rest + 1e-9 < want then
      s.active = nil
      return { broken = r, backlash = a.drawn * ritual.BACKLASH, why = "too little " .. colour .. " mana" }
    end
  end
  a.elapsed = a.elapsed + step
  if a.elapsed + 1e-9 < r.channel then return nil end
  s.active = nil
  local result = { done = r, from_land = a.from_land, blighted = mana.at(a.pos).blighted }
  if r.effect == "rain" or r.effect == "ward" then
    s.effects[r.effect] = { until_time = ctx.now + r.hours * 3600, pos = a.pos, radius = r.radius, rain = r.rain }
  end
  return result
end

-- What the rituals done still do, now: { rain = mm/h or nil, ward = { pos,
-- radius } or nil }; effects that have run out are forgotten.
function ritual.effects(s, now)
  local out = {}
  for kind, e in pairs(s.effects) do
    if now >= e.until_time then
      s.effects[kind] = nil
      out[kind .. "_ended"] = true
    elseif kind == "rain" then
      out.rain = e.rain
    elseif kind == "ward" then
      out.ward = { pos = e.pos, radius = e.radius }
    end
  end
  return out
end

-- The circle under way, as a share done (or nil).
function ritual.progress(s)
  local a = s.active
  if not a then return nil end
  return a.elapsed / ritual.get(a.id).channel
end

function ritual.line(s, pk)
  local r = ritual.rituals()[s.chosen] or ritual.rituals()[1]
  local parts = {}
  for id, n in pairs(r.needs or {}) do
    parts[#parts + 1] = string.format("%s %d/%d", items.name(id):lower(), pack.count(pk, id), n)
  end
  for colour, n in pairs(r.mana) do parts[#parts + 1] = colour .. " " .. n end
  table.sort(parts)
  local p = ritual.progress(s)
  if p then return string.format("Ritual: %s %d%% - keep still", ritual.get(s.active.id).name, math.floor(p * 100)) end
  return string.format("Ritual: %s (%s; %s, %d s) - / to begin, ' for the next", r.name, table.concat(parts, ", "),
    r.timing, r.channel)
end

return ritual
