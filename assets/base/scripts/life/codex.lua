-- SPDX-License-Identifier: MIT
-- The Codex (concept 7.3, 3 Naturalism): the mage's natural history of the
-- plants, fungi, animals and insects met. An entry grows Seen -> Observed ->
-- Studied -> Understood. What comes into sight is seen; looking at it a few
-- seconds (longer for animals) observes it, and the Codex then warns of its
-- dangers; taking a sample (harvesting, butchering, picking) studies it, and
-- insects, which cannot be sampled yet, are studied by watching them long.
-- Three samples (or long watching) understand it, and the right method then
-- gives a quarter more. Each level reveals more on the entry's page.
local fauna = require("life.fauna")
local items = require("life.items")
local plants = require("life.plants")
local smalllife = require("life.smalllife")
local news = require("player.news")

local codex = {}

codex.LEVELS = { "seen", "observed", "studied", "understood" }
codex.SEEN, codex.OBSERVED, codex.STUDIED, codex.UNDERSTOOD = 1, 2, 3, 4
codex.LOOK = { plant = 3, fungus = 3, insect = 3, animal = 8 } -- seconds of looking that observe
codex.WATCH = { studied = 30, understood = 90 } -- seconds of watching insects
codex.SAMPLES = 3 -- samples that understand
codex.METHOD = 1.25 -- the yield of the right method
codex.RANGE = { plant = 40, fungus = 12, insect = 25, animal = 200 } -- metres within which it is seen
codex.FIELD = 0.6 -- radians off the middle of the view within which a thing is seen
codex.CENTRE = 0.12 -- radians off the middle of the view (more for big things) that look at it
codex.SCAN_EVERY = 0.25 -- seconds between looks around

local LAYER_WORD = { tree = "a tree", shrub = "a shrub", ground = "a low plant" }
local HABITAT_WORD = { forest = "forests", open = "open land", wetland = "wetlands", dry = "dry land",
  cold = "cold land", deadwood = "dead trees" }
local FUNGUS_WHERE = { forest = "in forests", open = "on open land", deadwood = "on dead trees" }
local ACTIVITY_WORD = { day = "by day", night = "at night", twilight = "at dusk, by night and at dawn",
  any = "at any hour" }
local SEASONS = { "spring", "summer", "autumn", "winter" }
local WHEN_WORD = { fruit = " (in fruit)", flowers = " (in flower)", leaves = " (in leaf)" }

local registry, sources -- id -> entry; item -> the entries it comes from

local function read(path)
  local text = assert(vfs.read(path))
  return assert(load(text, "=" .. path, "t", {}))()
end

-- All entries by id ("plant:oak", "fungus:fly_agaric", "animal:wolf",
-- "insect:bee"): { id, kind, species, name, data }.
function codex.entries()
  if registry then return registry end
  local all, from = {}, {}
  local function add(kind, id, name, data)
    local key = kind .. ":" .. id
    all[key] = { id = key, kind = kind, species = id, name = name or id, data = data }
    return key
  end
  local function source(item, key)
    from[item] = from[item] or {}
    from[item][#from[item] + 1] = key
  end
  local cycles = plants.cycles()
  for _, f in ipairs(read("data/flora.lua")) do
    local key = add("plant", f.id, f.name, f)
    for _, part in ipairs((cycles[f.id] or {}).parts or {}) do source(part.item, key) end
  end
  for _, f in ipairs(smalllife.fungi()) do source(f.item, add("fungus", f.id, f.name, f)) end
  for id, sp in pairs(fauna.species()) do
    local key = add("animal", id, sp.name, sp)
    for item in pairs(sp.products or {}) do source(item, key) end
  end
  for _, b in ipairs(read("data/insects.lua")) do add("insect", b.id, b.name, b) end
  registry, sources = all, from
  return registry
end

function codex.new(saved)
  local c = { known = {}, news = news.new(), since = codex.SCAN_EVERY, focus = nil, order = 0, open = nil }
  local all = codex.entries()
  for id, k in pairs(type(saved) == "table" and type(saved.known) == "table" and saved.known or {}) do
    if all[id] and type(k) == "table" then
      local level = math.floor(tonumber(k.level) or 1)
      c.known[id] = { level = math.max(codex.SEEN, math.min(level, codex.UNDERSTOOD)),
        watched = tonumber(k.watched) or 0, samples = math.floor(tonumber(k.samples) or 0),
        order = tonumber(k.order) or 0 }
      c.order = math.max(c.order, c.known[id].order)
    end
  end
  return c
end

function codex.snapshot(c)
  local out = { known = {} }
  for id, k in pairs(c.known) do
    out.known[id] = { level = k.level, watched = k.watched, samples = k.samples, order = k.order }
  end
  return out
end

-- How well an entry is known: 0 (not at all) .. 4 (understood).
function codex.level(c, id)
  return c.known[id] and c.known[id].level or 0
end

-- How many entries are known at each level or better: { seen, observed, studied, understood }.
function codex.counts(c)
  local out = { 0, 0, 0, 0 }
  for _, k in pairs(c.known) do
    for l = 1, k.level do out[l] = out[l] + 1 end
  end
  return out
end

-- ---- What is dangerous ----

local function parts_of(e)
  return e.kind == "plant" and ((plants.cycles()[e.species] or {}).parts or {}) or {}
end

-- The Codex's warning about an entry, or nil.
function codex.danger(id)
  local e = codex.entries()[id]
  if not e then return nil end
  if e.kind == "fungus" then
    local it = items.get(e.data.item)
    if it and (it.poison or 0) > 0 then return "poisonous" end
  elseif e.kind == "plant" then
    for _, part in ipairs(parts_of(e)) do
      local it = items.get(part.item)
      if it and (it.poison or 0) > 0 then return "its " .. it.name:lower() .. " are poisonous" end
    end
  elseif e.kind == "animal" then
    if (e.data.aggression or 0) >= 0.5 then return "it may attack" end
  else
    return e.data.danger
  end
  return nil
end

-- Whether the Codex has warned of an item's dangers: an item that comes from
-- no entry is plain to see; else one of the entries it comes from must be
-- observed.
function codex.warned(c, item)
  codex.entries()
  local from = sources[item]
  if not from then return true end
  for _, id in ipairs(from) do
    if codex.level(c, id) >= codex.OBSERVED then return true end
  end
  return false
end

-- ---- Learning ----

local function raise(c, id, level)
  local k = c.known[id]
  if k and k.level >= level then return false end
  if not k then
    k = { level = 0, watched = 0, samples = 0, order = 0 }
    c.known[id] = k
  end
  k.level = level
  c.order = c.order + 1
  k.order = c.order
  if level > codex.SEEN then -- (much is seen at once; the Codex counts it quietly)
    local e = codex.entries()[id]
    local warn = codex.danger(id)
    news.say(c.news, "Codex: " .. e.name .. " " .. codex.LEVELS[level] .. (warn and (" - " .. warn .. "!") or ""))
  end
  return true
end

-- What the mage sees now: candidates { id, angle (radians off the middle of
-- the view), allow (radians the thing itself spans), dist (metres) }. All in
-- sight are seen; the one looked at (nearest the middle) is the focus.
function codex.see(c, candidates)
  local all = codex.entries()
  local best, score
  for _, cand in ipairs(candidates) do
    local e = all[cand.id]
    if e and cand.dist <= codex.RANGE[e.kind] and cand.angle <= codex.FIELD then
      raise(c, cand.id, codex.SEEN)
      local off = cand.angle - (cand.allow or 0)
      if off <= codex.CENTRE and (not score or off < score) then best, score = cand.id, off end
    end
  end
  c.focus = best
end

-- Time spent looking at the focus: observed after LOOK seconds; insects,
-- watched long, are studied and understood.
function codex.watch(c, dt)
  local id = c.focus
  local k = id and c.known[id]
  if not k then return end
  local e = codex.entries()[id]
  k.watched = k.watched + dt
  if k.watched >= codex.LOOK[e.kind] then raise(c, id, codex.OBSERVED) end
  if e.kind == "insect" then
    if k.watched >= codex.WATCH.studied then raise(c, id, codex.STUDIED) end
    if k.watched >= codex.WATCH.understood then raise(c, id, codex.UNDERSTOOD) end
  end
end

-- Samples were taken (harvested, butchered, picked) of each entry in ids:
-- studied, and understood after SAMPLES.
function codex.sampled(c, ids)
  local all = codex.entries()
  for _, id in ipairs(ids or {}) do
    if all[id] then
      raise(c, id, codex.STUDIED)
      local k = c.known[id]
      k.samples = k.samples + 1
      if k.samples >= codex.SAMPLES then raise(c, id, codex.UNDERSTOOD) end
    end
  end
end

-- The yield of what is taken from an entry: the right method, once
-- understood, gives a quarter more.
function codex.method(c, id)
  return codex.level(c, id) >= codex.UNDERSTOOD and codex.METHOD or 1
end

-- ---- Looking around ----

local function sub(a, b) return { a[1] - b[1], a[2] - b[2], a[3] - b[3] } end
local function len(v) return math.sqrt(v[1] * v[1] + v[2] * v[2] + v[3] * v[3]) end

-- The candidates around view = { eye = planet position, dir = { x, y, z }
-- (render axes), feet = planet position when on foot }; life = { smalllife }.
function codex.around(view, life, now)
  local out = {}
  local eye = world.to_local(view.eye)
  local dl = len(view.dir)
  if dl <= 0 then return out end
  local function consider(id, pos, size)
    local v = sub(world.to_local(pos), eye)
    local d = len(v)
    if d < 1e-3 then return end
    local cos = (v[1] * view.dir[1] + v[2] * view.dir[2] + v[3] * view.dir[3]) / (d * dl)
    out[#out + 1] = { id = id, dist = d, angle = math.acos(math.max(-1, math.min(1, cos))),
      allow = math.atan(size / 2, d) }
  end
  for _, pl in ipairs(world.plants(view.eye, codex.RANGE.plant)) do
    local h = pl.height or 1
    consider("plant:" .. pl.species, world.move(pl.pos, 0, 0, 0, h / 2), h)
  end
  for _, id in ipairs(sim.creatures_near(view.eye, codex.RANGE.animal)) do
    local cr = sim.creature(id)
    local sp = cr and fauna.species()[cr.species]
    if sp then consider("animal:" .. cr.species, world.move(cr.pos, 0, 0, 0, 0.4), sp.length or 1) end
  end
  local state = life and life.smalllife
  for _, sw in ipairs(state and state.swarms or {}) do
    consider("insect:" .. sw.kind, world.move(sw.anchor, 0, 0, 0, 1.4), 4)
  end
  if view.feet and state then -- the mushrooms of the patch, on the ground ahead
    local heading = math.atan(view.dir[1], -view.dir[3])
    local spot = world.move(view.feet, heading, 1.5, 0)
    spot.alt = world.ground_height(spot)
    for _, m in ipairs((smalllife.mushrooms(state, view.feet, now))) do consider("fungus:" .. m.fungus, spot, 1.5) end
  end
  return out
end

-- Real time passes: looks around now and then, watches the focus, and old
-- news goes.
function codex.look(c, dt, view, life, now)
  news.tick(c.news, dt)
  c.since = c.since + dt
  if c.since >= codex.SCAN_EVERY then
    c.since = 0
    codex.see(c, codex.around(view, life, now))
  end
  codex.watch(c, dt)
end

-- ---- Pages ----

local function season_span(w)
  if not w then return nil end
  if w[2] - w[1] >= 4 then return "all year" end
  local a = SEASONS[math.floor(w[1]) % 4 + 1]
  local b = SEASONS[math.floor(math.max(w[2] - 0.01, w[1])) % 4 + 1]
  return a == b and a or (a .. " to " .. b)
end

local function item_note(id)
  local it = items.get(id)
  if not it then return nil end
  local bits = {}
  if it.kcal then bits[#bits + 1] = it.kcal .. " kcal" end
  if (it.poison or 0) > 0 then bits[#bits + 1] = "poisonous" end
  if (it.raw or 0) > 0 then bits[#bits + 1] = "safer cooked" end
  if it.cooked then bits[#bits + 1] = "cooks into " .. items.name(it.cooked):lower() end
  if it.fuel then bits[#bits + 1] = "burns" end
  if #bits == 0 then return nil end
  return it.name .. ": " .. table.concat(bits, ", ")
end

local function deep_note(id)
  local it = items.get(id)
  if not it then return nil end
  local bits = {}
  for what, amount in pairs(it.cures or {}) do
    bits[#bits + 1] = string.format("eases %s%s", what == "sick" and "sickness" or what,
      amount >= 0.3 and " well" or "")
  end
  table.sort(bits)
  if it.brew then bits[#bits + 1] = "steeped in boiled water: " .. items.name(it.brew):lower() end
  if #bits == 0 then return nil end
  return it.name .. ": " .. table.concat(bits, ", ")
end

local function names_of(ids, kind)
  local out = {}
  for id in pairs(ids) do
    local e = codex.entries()[kind .. ":" .. id]
    out[#out + 1] = e and e.name:lower() or id
  end
  table.sort(out)
  return table.concat(out, ", ")
end

local function habitats(h)
  local list = {}
  for k, v in pairs(h or {}) do
    if v >= 0.5 then list[#list + 1] = HABITAT_WORD[k] or k end
  end
  table.sort(list)
  return table.concat(list, ", ")
end

local function page_plant(e, level, lines)
  local f, cycle = e.data, plants.cycles()[e.species] or {}
  lines[#lines + 1] = e.name .. ": " .. (LAYER_WORD[f.layer] or "a plant")
  if level >= codex.OBSERVED then
    if f.temperature and f.rain then
      lines[#lines + 1] = string.format("Grows where the year is %.0f to %.0f C with %.0f to %.0f mm of rain%s",
        f.temperature[1], f.temperature[3], f.rain[1], f.rain[3], f.wet and ", in wetlands and on banks" or "")
    end
    local times = {}
    if cycle.flowers then times[#times + 1] = "flowers " .. season_span(cycle.flowers) end
    if cycle.fruit then times[#times + 1] = "fruit " .. season_span(cycle.fruit) end
    if #times > 0 then lines[#lines + 1] = "In " .. table.concat(times, "; ") end
  end
  if level >= codex.STUDIED then
    local gives = {}
    for _, part in ipairs(cycle.parts or {}) do
      local when = part.when and WHEN_WORD[part.when] or ""
      gives[#gives + 1] = items.name(part.item):lower() .. when .. (part.whole and " (felled)" or "")
    end
    if #gives > 0 then lines[#lines + 1] = "Gives " .. table.concat(gives, ", ") end
    for _, part in ipairs(cycle.parts or {}) do lines[#lines + 1] = item_note(part.item) end
  end
  if level >= codex.UNDERSTOOD then
    for _, part in ipairs(cycle.parts or {}) do lines[#lines + 1] = deep_note(part.item) end
    if cycle.parts and #cycle.parts > 0 then lines[#lines + 1] = "Taken the right way, it gives a quarter more" end
  end
end

local function page_fungus(e, level, lines)
  local f = e.data
  lines[#lines + 1] = e.name .. ": a fungus"
  if level >= codex.OBSERVED then
    lines[#lines + 1] = "Fruits " .. (FUNGUS_WHERE[f.habitat] or f.habitat) .. ", " .. season_span(f.season)
      .. ", after rain"
  end
  if level >= codex.STUDIED then lines[#lines + 1] = item_note(f.item) end
  if level >= codex.UNDERSTOOD then
    lines[#lines + 1] = deep_note(f.item)
    lines[#lines + 1] = "Picked the right way, a patch gives a quarter more"
  end
end

local function page_animal(e, level, lines)
  local sp = e.data
  lines[#lines + 1] = e.name .. ": an animal"
  if level >= codex.OBSERVED then
    local where = habitats(sp.habitat)
    lines[#lines + 1] = "Lives in " .. (where ~= "" and where or "many places") .. ", about "
      .. (ACTIVITY_WORD[sp.activity or "any"] or "at any hour")
    if sp.group then lines[#lines + 1] = string.format("In groups of %.0f to %.0f", sp.group[1], sp.group[2]) end
    if sp.eats then lines[#lines + 1] = "Hunts " .. names_of(sp.eats, "animal") end
  end
  if level >= codex.STUDIED then
    local gives = {}
    for item, n in pairs(sp.products or {}) do gives[#gives + 1] = items.name(item):lower() .. " " .. n end
    table.sort(gives)
    if #gives > 0 then lines[#lines + 1] = "Butchered, gives " .. table.concat(gives, ", ") end
    lines[#lines + 1] = string.format("Runs up to %.0f m/s; smells you %.0f m downwind", sp.run or 0, sp.smell or 0)
  end
  if level >= codex.UNDERSTOOD then
    if (sp.fear_of_fire or 0) >= 0.5 then lines[#lines + 1] = "Afraid of fire" end
    for harm, r in pairs(sp.resist or {}) do
      if r >= 0.5 then lines[#lines + 1] = "Shrugs off " .. harm end
    end
    if sp.products then lines[#lines + 1] = "Butchered the right way, it gives a quarter more" end
  end
end

local function page_insect(e, level, lines)
  lines[#lines + 1] = e.name .. ": an insect"
  for l = codex.OBSERVED, level do
    for _, text in ipairs(e.data[codex.LEVELS[l]] or {}) do lines[#lines + 1] = text end
  end
end

-- The page of an entry as far as it is known: lines of text (none when not
-- known at all).
function codex.page(c, id)
  local e = codex.entries()[id]
  local level = codex.level(c, id)
  if not e or level == 0 then return {} end
  local lines = {}
  local fn = ({ plant = page_plant, fungus = page_fungus, animal = page_animal, insect = page_insect })[e.kind]
  fn(e, level, lines)
  local warn = level >= codex.OBSERVED and codex.danger(id)
  if warn then lines[#lines + 1] = "Beware: " .. warn end
  if level < codex.UNDERSTOOD then
    lines[#lines + 1] = level == codex.SEEN and "Look at it longer to learn more"
      or (e.kind == "insect" and "Watch it longer to learn more" or "Take a sample to learn more")
  end
  local out = {}
  for _, l in ipairs(lines) do
    if l then out[#out + 1] = l end
  end
  out[1] = out[1] .. " (" .. codex.LEVELS[level] .. ")"
  return out
end

-- ---- Reading ----

-- The known entries, last learned first.
function codex.list(c)
  local out = {}
  for id in pairs(c.known) do out[#out + 1] = id end
  table.sort(out, function(a, b) return c.known[a].order > c.known[b].order end)
  return out
end

-- Opens the Codex at the entry last learned, turns to the next, and closes
-- after the last.
function codex.browse(c)
  local list = codex.list(c)
  if #list == 0 then
    news.say(c.news, "The Codex is empty: look about you")
    return nil
  end
  if not c.open then
    c.open = 1
  elseif c.open >= #list then
    c.open = nil
  else
    c.open = c.open + 1
  end
  return c.open and list[c.open]
end

function codex.hud_lines(c)
  local lines = news.lines(c.news)
  local id = c.focus
  local k = id and c.known[id]
  if k and k.level < codex.OBSERVED then
    local e = codex.entries()[id]
    lines[#lines + 1] = string.format("Looking at %s %d%%", e.name:lower(),
      math.floor(math.min(k.watched / codex.LOOK[e.kind], 1) * 100))
  end
  if c.open then
    local list = codex.list(c)
    local n = codex.counts(c)
    lines[#lines + 1] = string.format("Codex %d/%d: %d seen, %d observed, %d studied, %d understood (J: next)",
      math.min(c.open, #list), #list, n[1], n[2], n[3], n[4])
    for _, l in ipairs(codex.page(c, list[math.min(c.open, #list)])) do lines[#lines + 1] = "  " .. l end
  end
  return lines
end

return codex
