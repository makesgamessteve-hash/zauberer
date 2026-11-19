-- SPDX-License-Identifier: MIT
-- The history database (concept 10.11): one schema whichever way the world
-- was peopled. Entities (cultures, languages, faiths, polities, dynasties,
-- settlements, figures, artifacts, sites) have stable ids "kind:n"; events
-- have a year, a place, participants, a cause (an earlier event), an outcome
-- and an importance. Minor events are pruned into summaries ("border
-- skirmishes, 1120-1180"), but never those the present refers to.
local job = require("people.job")

local history = {}

history.KINDS = { "culture", "language", "faith", "polity", "dynasty", "settlement", "figure", "artifact", "site" }

function history.new(present_year, variant)
  local db = { year = present_year, variant = variant or "settled", events = {}, by_id = {}, next_event = 1,
    referenced = {} }
  for _, k in ipairs(history.KINDS) do db[k] = {} end
  return db
end

-- Adds an entity of a kind; returns it with its id.
function history.add(db, kind, rec)
  local list = assert(db[kind], "unknown kind " .. tostring(kind))
  rec.id = kind .. ":" .. (#list + 1)
  list[#list + 1] = rec
  db.by_id[rec.id] = rec
  return rec
end

function history.get(db, id)
  return db.by_id[id]
end

-- The kind of an entity or event by its id ("culture", ..., or "event").
function history.kind_of(id)
  return type(id) == "string" and id:match("^(%a+):") or nil
end

-- Records an event: { year, kind, place (a settlement or site id, or a cell),
-- participants (ids), cause (an event id), outcome (text or table), importance
-- 0..1, title }. Returns it with its id "event:n".
function history.event(db, ev)
  assert(ev.year and ev.kind, "an event needs a year and a kind")
  ev.id = "event:" .. db.next_event
  db.next_event = db.next_event + 1
  ev.participants = ev.participants or {}
  ev.importance = ev.importance or 0.5
  db.events[#db.events + 1] = ev
  db.by_id[ev.id] = ev
  return ev
end

-- Marks an event or entity as referenced by the present (a ruin, a tomb, a
-- name, a grudge): it is kept whatever its importance.
function history.refer(db, id)
  db.referenced[id] = true
end

-- Events in time order, optionally those with a participant or at a place.
function history.events(db, filter)
  local out = {}
  for _, ev in ipairs(db.events) do
    local ok = true
    if filter and filter.participant then
      ok = false
      for _, p in ipairs(ev.participants) do
        if p == filter.participant then ok = true end
      end
    end
    if ok and filter and filter.place and ev.place ~= filter.place then ok = false end
    if ok and filter and filter.kind and ev.kind ~= filter.kind then ok = false end
    if ok then out[#out + 1] = ev end
  end
  table.sort(out, function(a, b)
    if a.year ~= b.year then return a.year < b.year end
    return a.id < b.id
  end)
  return out
end

-- Folds events below an importance into summaries per kind, first
-- participant and century; referenced events and those a kept event names as its cause
-- stay. Returns how many were folded.
function history.prune(db, min_importance)
  local keep = {}
  for _, ev in ipairs(db.events) do
    if ev.importance >= min_importance or db.referenced[ev.id] then keep[ev.id] = true end
  end
  local changed = true
  while changed do -- causes of kept events are kept
    changed = false
    for _, ev in ipairs(db.events) do
      if keep[ev.id] and ev.cause and not keep[ev.cause] then
        keep[ev.cause], changed = true, true
      end
    end
  end
  local groups, order, out, folded = {}, {}, {}, 0
  for _, ev in ipairs(db.events) do
    job.pace(0.05)
    if keep[ev.id] then
      out[#out + 1] = ev
    else
      folded = folded + 1
      local key = ev.kind .. "|" .. tostring(ev.participants[1]) .. "|" .. (ev.year // 100)
      local g = groups[key]
      if not g then
        g = { kind = ev.kind, participants = { ev.participants[1] }, from = ev.year, to = ev.year, count = 0,
          place = ev.place }
        groups[key] = g
        order[#order + 1] = key
      end
      g.from, g.to, g.count = math.min(g.from, ev.year), math.max(g.to, ev.year), g.count + 1
      db.by_id[ev.id] = nil
    end
  end
  db.events = out
  for _, key in ipairs(order) do
    local g = groups[key]
    local summary = history.event(db, { year = g.from, kind = "summary", participants = g.participants,
      place = g.place, importance = 0, outcome = { of = g.kind, count = g.count, from = g.from, to = g.to } })
    summary.title = history.summary_title(g)
  end
  return folded
end

local PLURAL = { skirmish = "border skirmishes", raid = "raids", famine = "lean years", flood = "floods",
  founding = "foundings", reign = "reigns", birth = "births", death = "deaths" }

function history.summary_title(g)
  local what = PLURAL[g.kind] or (g.kind .. "s")
  if g.from == g.to then return string.format("%s, %d", what, g.from) end
  return string.format("%s, %d-%d", what, g.from, g.to)
end

-- The database as plain tables for a save (entities keep their ids; links
-- are ids, never tables).
function history.snapshot(db)
  local out = { year = db.year, variant = db.variant, events = db.events, next_event = db.next_event,
    referenced = db.referenced }
  for _, k in ipairs(history.KINDS) do out[k] = db[k] end
  return out
end

function history.restore(saved)
  if type(saved) ~= "table" or type(saved.year) ~= "number" or type(saved.events) ~= "table" then return nil end
  local db = history.new(saved.year, saved.variant)
  db.next_event = tonumber(saved.next_event) or (#saved.events + 1)
  db.referenced = type(saved.referenced) == "table" and saved.referenced or {}
  for _, k in ipairs(history.KINDS) do
    db[k] = type(saved[k]) == "table" and saved[k] or {}
    for _, rec in ipairs(db[k]) do db.by_id[rec.id] = rec end
  end
  for _, ev in ipairs(saved.events) do
    if type(ev) == "table" and ev.id then
      db.events[#db.events + 1] = ev
      db.by_id[ev.id] = ev
    end
  end
  return db
end

return history
