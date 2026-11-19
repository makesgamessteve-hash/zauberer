-- SPDX-License-Identifier: MIT
-- Lore from the history database (concept 10.11): chronicles and books of
-- kings told from a people's side (the same war a glorious victory in one,
-- a treacherous massacre in the other), legends that drift from the truth
-- with time and distance, inscriptions in a people's language and script,
-- what people know of their land's past, and the lore journal: the Codex's
-- Peoples, Faiths, Realms and History, learnt from books, talk and visits,
-- never all at once.
local history = require("people.history")
local language = require("people.language")
local peopling = require("people.peopling")

local lore = {}

lore.SECTIONS = { culture = "Peoples", faith = "Faiths", polity = "Realms", event = "History", site = "History",
  figure = "History", dynasty = "Realms", settlement = "Realms", artifact = "History", language = "Peoples" }

local function side(db, teller_culture, ev)
  local w, l = history.get(db, ev.winner), history.get(db, ev.loser)
  if w and w.culture == teller_culture then return "winner", w, l end
  if l and l.culture == teller_culture then return "loser", w, l end
  return "neutral", w, l
end

-- A war told by a people: { title, lines, side }.
function lore.war_account(db, war_id, teller_culture)
  local war = history.get(db, war_id)
  local battle = history.get(db, war.battle)
  local site = history.get(db, war.site)
  local who, w, l = side(db, teller_culture, war)
  local lines = {}
  local fallen = battle and battle.fallen or 0
  if who == "winner" then
    lines[1] = string.format("In the year %d the faithless men of %s rose against us over %s.", war.year, l.name,
      war.cause_text)
    lines[2] = string.format("At %s the armies of %s won a glorious victory; their dead lay in thousands.",
      site.name, w.name)
    lines[3] = "So the land was kept, as it is kept today."
  elseif who == "loser" then
    lines[1] = string.format("In the year %d %s fell upon our people without cause, coveting %s.", war.year, w.name,
      war.cause_text == "land" and "our fields" or ("our " .. war.cause_text))
    lines[2] = string.format("At %s they slaughtered %d of ours by treachery, and our martyrs are remembered.",
      site.name, fallen)
    lines[3] = string.format("What %s holds was taken, not won.", w.name)
  else
    lines[1] = string.format("In %d %s and %s went to war over %s.", war.year, w.name, l.name, war.cause_text)
    lines[2] = string.format("%s won at %s, where some %d fell.", w.name, site.name, fallen)
  end
  return { title = (who == "loser" and "The Treachery of " or "The Victory at ") .. site.name, lines = lines,
    side = who }
end

-- A realm's book of kings, as a people tells it: its own rulers as they were
-- remembered, an enemy's blackened.
function lore.book_of_kings(db, polity_id, teller_culture)
  local p = history.get(db, polity_id)
  local d = peopling.data()
  local teller_realms = {}
  for _, q in ipairs(db.polity) do
    if q.culture == teller_culture then teller_realms[q.id] = true end
  end
  local hostile = false
  if p.culture == teller_culture then teller_realms = {} end -- its own people tell it straight
  for _, g in ipairs(p.grudges or {}) do
    if teller_realms[g.against] then hostile = true end
  end
  for rid in pairs(teller_realms) do
    for _, g in ipairs(history.get(db, rid).grudges or {}) do
      if g.against == p.id then hostile = true end
    end
  end
  local lines = { "The Book of the Kings of " .. p.name }
  for _, dyn in ipairs(db.dynasty) do
    if dyn.polity == p.id then
      lines[#lines + 1] = dyn.name .. ", from " .. dyn.from
      for i, rid in ipairs(dyn.rulers or {}) do
        local r = history.get(db, rid)
        local epithet = r.epithet
        if hostile and r.good then
          epithet = d.peoples.epithets.bad[1 + i % #d.peoples.epithets.bad]
        end
        lines[#lines + 1] = string.format("  %s %s, %d-%d", r.name, epithet, r.reign_from, r.reign_to)
      end
    end
  end
  return lines
end

-- How far a tale has drifted: 0 the truth .. 1 a myth (age in years,
-- distance in metres).
function lore.drift(age, distance)
  return math.max(0, math.min(1, age / 400 + distance / 2000000))
end

-- An event as a legend told far away or long after.
function lore.legend(db, event_id, teller_culture, age, distance)
  local ev = history.get(db, event_id)
  local dr = lore.drift(age, distance)
  local when = dr > 0.5 and "In the days of old" or ("In the year " .. ev.year)
  local faith
  for _, f in ipairs(db.faith) do
    if f.culture == teller_culture then faith = f end
  end
  faith = faith or db.faith[1]
  if ev.kind == "eruption" or ev.kind == "earthquake" then
    if dr > 0.3 then return when .. ", " .. faith.deity .. " was angered and the mountain burned the wicked." end
    return when .. ", the mountain woke in fire."
  elseif ev.kind == "battle" or ev.kind == "war" then
    local battle = ev.kind == "war" and history.get(db, ev.battle) or ev
    local fallen = math.floor((battle.fallen or 1000) * (1 + 4 * dr))
    local w = history.get(db, ev.winner)
    local text = string.format("%s, %d fell where %s won.", when, fallen, w.name)
    if dr > 0.6 then text = text .. " Some say the gods themselves fought there." end
    return text
  elseif ev.kind == "revelation" then
    return when .. ", a holy one heard the voice of " .. faith.deity .. (dr > 0.4 and " in a pillar of light." or ".")
  end
  local text = when .. ", " .. (ev.title or ev.kind)
  if dr > 0.6 then text = text .. ", and nothing was the same after." else text = text .. "." end
  return text
end

-- An inscription at a site: the words in its people's language and script,
-- and what they say.
function lore.inscription(db, site_id)
  local site = history.get(db, site_id)
  local lang_id, meanings
  if site.language then
    lang_id = site.language
  else
    local cell_culture = db.cell_culture[site.cell]
    local cu = cell_culture and history.get(db, cell_culture) or db.culture[1]
    lang_id = cu.language
  end
  if site.kind == "tomb" then meanings = { "here", "king", "sleep", "great" }
  elseif site.kind == "battlefield" then meanings = { "here", "blood", "victory", "banner" }
  elseif site.kind == "elder ruin" then meanings = { "light", "weave", "holy", "first" }
  else meanings = { "holy", "water", "god", "peace" } end
  local lang = peopling.language(db, lang_id)
  local native, gloss = language.phrase(lang, meanings)
  return { script = lang.script, native = native, gloss = gloss, language = lang_id }
end

-- What someone of a people and faith living at a place knows: topics of
-- lines. npc: { culture, faith, pos, realm }.
function lore.knowledge(db, npc)
  local out = { history = {}, faith = {}, grudges = {}, songs = {}, rumours = {} }
  local realm = npc.realm and history.get(db, npc.realm)
  for _, wid in ipairs(db.wars or {}) do
    local war = history.get(db, wid)
    if war and realm and (war.winner == realm.id or war.loser == realm.id) then
      local acc = lore.war_account(db, wid, npc.culture)
      out.history[#out.history + 1] = acc.lines[1] .. " " .. acc.lines[2]
      if acc.side == "loser" then
        out.grudges[#out.grudges + 1] = "We have not forgotten " .. history.get(db, war.site).name .. "."
      end
    end
  end
  local f = npc.faith and history.get(db, npc.faith)
  if f then
    out.faith[1] = string.format("We keep %s; we pray %s and bury our dead by %s.", f.name, f.prayers, f.burial)
    out.faith[2] = "Magic is " .. f.magic .. " to us."
    out.faith[3] = "We hold the " .. f.sacred.animal .. " and the " .. f.sacred.plant .. " sacred."
  end
  for _, fig in ipairs(db.figure) do
    if fig.culture == npc.culture and fig.role == "general" and #out.songs < 2 then
      out.songs[#out.songs + 1] = "They sing of " .. fig.name .. ", who led us to victory."
    end
  end
  for _, ev in ipairs(db.events) do
    if ev.year >= db.year - 10 and ev.kind ~= "summary" and #out.rumours < 3 then
      out.rumours[#out.rumours + 1] = (ev.title or ev.kind) .. ", they say."
    end
  end
  return out
end

-- The lore journal: what the heirs have learnt.
function lore.new(saved)
  local j = { known = {} }
  for id, src in pairs(type(saved) == "table" and type(saved.known) == "table" and saved.known or {}) do
    if type(id) == "string" and type(src) == "string" then j.known[id] = src end
  end
  return j
end

function lore.snapshot(j)
  return { known = j.known }
end

-- Learns of an entity or event from a source ("book", "talk", "visit").
-- Returns its Codex section, or nil if it was known already or is unknown.
function lore.learn(j, db, id, source)
  local rec = history.get(db, id)
  if not rec or j.known[id] then return nil end
  j.known[id] = source
  return lore.SECTIONS[history.kind_of(id)] or "History"
end

-- How many of each section are known.
function lore.counts(j, db)
  local out = { Peoples = 0, Faiths = 0, Realms = 0, History = 0 }
  for id in pairs(j.known) do
    if history.get(db, id) then
      local sec = lore.SECTIONS[history.kind_of(id)] or "History"
      out[sec] = out[sec] + 1
    end
  end
  return out
end

-- The page of a known entry, nil when unknown.
function lore.page(j, db, id)
  if not j.known[id] then return nil end
  local rec = history.get(db, id)
  if not rec then return nil end
  local lines = {}
  local kind = history.kind_of(id)
  if kind == "culture" then
    lines[1] = "The " .. rec.name .. ": " .. rec.subsistence .. ", prizing " .. rec.values[1] .. " and "
      .. rec.values[2]
    lines[2] = "Magic is " .. rec.magic .. " among them; " .. rec.hospitality .. "."
    lines[3] = "They weave " .. rec.textiles[1] .. " and play the " .. rec.instruments[1] .. "."
  elseif kind == "faith" then
    lines[1] = rec.name .. ": " .. rec.concept .. ", " .. rec.imagery
    lines[2] = "Magic is " .. rec.magic .. "; " .. rec.prayers .. "; " .. rec.diet .. "."
  elseif kind == "polity" then
    lines[1] = rec.name .. ", a " .. rec.type .. " ruled by its " .. rec.title
    lines[2] = "Its law: " .. table.concat(rec.laws, "; ")
  else
    lines[1] = rec.title or rec.name or id
  end
  return lines
end

return lore
