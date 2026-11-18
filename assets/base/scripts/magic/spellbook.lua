-- SPDX-License-Identifier: MIT
-- The spellbook (concept 3.3): the journal of weaves the mage has discovered,
-- with how often each was cast (familiarity), the prepared weaves inscribed in
-- the book, the left and right hand slots and a quick wheel of up to eight.
local weave = require("magic.weave")

local spellbook = {}

spellbook.WHEEL = 8
spellbook.MAX_PREPARED = 32

function spellbook.new()
  return { journal = {}, order = {}, prepared = {}, hands = {}, wheel = {}, news = {} }
end

-- The weave without its power (what the journal and the slots keep).
local function shape_of(w)
  local mods = {}
  for i, m in ipairs(w.modifiers or {}) do mods[i] = m end
  return { verb = w.verb, colors = { w.colors[1], w.colors[2] }, form = w.form, modifiers = mods }
end

function spellbook.entry(book, w)
  return book.journal[weave.key(w)]
end

function spellbook.casts(book, w)
  local e = spellbook.entry(book, w)
  return e and e.casts or 0
end

-- Writes a weave into the journal (from a book, a teacher or a first cast);
-- returns the entry and whether it is new.
function spellbook.learn(book, w, how)
  local key = weave.key(w)
  local e = book.journal[key]
  if e then return e, false end
  local name = weave.name(w)
  e = { key = key, name = name, weave = shape_of(w), casts = 0, how = how or "cast" }
  book.journal[key] = e
  book.order[#book.order + 1] = key
  book.news[#book.news + 1] = "You discovered " .. name
  return e, true
end

-- A cast of a weave: a valid combination not yet in the journal is
-- discovered; familiarity grows. Returns the entry, whether it was
-- discovered, and the tier reached when a new one is.
function spellbook.record_cast(book, w)
  local e, new = spellbook.learn(book, w, "cast")
  local before = weave.tier(e.casts)
  e.casts = e.casts + 1
  local after = weave.tier(e.casts)
  if after ~= before then
    book.news[#book.news + 1] = e.name .. " is now " .. after.name
  end
  return e, new, after ~= before and after.name or nil
end

-- Inscribes a known weave as prepared (once); returns its index.
function spellbook.prepare(book, w)
  local key = weave.key(w)
  if not book.journal[key] then return nil, "not in the journal" end
  for i, p in ipairs(book.prepared) do
    if weave.key(p) == key then return i end
  end
  if #book.prepared >= spellbook.MAX_PREPARED then return nil, "the book is full" end
  book.prepared[#book.prepared + 1] = shape_of(w)
  return #book.prepared
end

-- Puts prepared weave i in a hand ("left" or "right") or wheel slot (1 .. 8).
function spellbook.assign(book, slot, i)
  if not book.prepared[i] then return false, "no prepared weave " .. tostring(i) end
  if slot == "left" or slot == "right" then
    book.hands[slot] = i
  elseif type(slot) == "number" and slot >= 1 and slot <= spellbook.WHEEL and slot % 1 == 0 then
    book.wheel[slot] = i
  else
    return false, "no such slot"
  end
  return true
end

-- The prepared weave in a hand or wheel slot (nil when empty).
function spellbook.in_slot(book, slot)
  local i = (slot == "left" or slot == "right") and book.hands[slot] or book.wheel[slot]
  return i and book.prepared[i] or nil
end

-- Messages since last asked ("You discovered Hold Water").
function spellbook.take_news(book)
  local out = book.news
  book.news = {}
  return out
end

-- The journal in the order of discovery, with names, casts and tiers.
function spellbook.list(book)
  local out = {}
  for _, key in ipairs(book.order) do
    local e = book.journal[key]
    out[#out + 1] = { name = e.name, key = key, casts = e.casts, tier = weave.tier(e.casts).name }
  end
  return out
end

-- Plain values for saves, and back.
function spellbook.snapshot(book)
  local journal = {}
  for _, key in ipairs(book.order) do
    local e = book.journal[key]
    journal[#journal + 1] = { weave = e.weave, casts = e.casts, how = e.how }
  end
  return { journal = journal, prepared = book.prepared, hands = book.hands, wheel = book.wheel }
end

function spellbook.restore(snap)
  local book = spellbook.new()
  for _, e in ipairs(type(snap) == "table" and snap.journal or {}) do
    local w = { verb = e.weave.verb, colors = e.weave.colors, form = e.weave.form, modifiers = e.weave.modifiers,
      power = 1 }
    if weave.validate(w) then
      local entry = spellbook.learn(book, w, e.how)
      entry.casts = tonumber(e.casts) or 0
    end
  end
  book.news = {}
  for _, p in ipairs(type(snap) == "table" and snap.prepared or {}) do
    spellbook.prepare(book, p)
  end
  for slot, i in pairs(type(snap) == "table" and snap.hands or {}) do spellbook.assign(book, slot, i) end
  for slot, i in pairs(type(snap) == "table" and snap.wheel or {}) do spellbook.assign(book, tonumber(slot), i) end
  return book
end

return spellbook
