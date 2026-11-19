-- SPDX-License-Identifier: MIT
-- Lore from the peopled world's history: one war told by both sides and by
-- a stranger, books of kings, legends drifting with time and distance,
-- inscriptions in a people's script, what people know, and the journal.
scenario { name = "lore" }
local history = require("people.history")
local chronicle = require("people.chronicle")
local lore = require("people.lore")

local anchor = world.position(0, 6506, 6025, 128, 128, 0)
local db = chronicle.settled_world(4242, { anchor = anchor })
local war = history.get(db, db.wars[1])
local winner, loser = history.get(db, war.winner), history.get(db, war.loser)
local stranger
for _, cu in ipairs(db.culture) do
  if cu.id ~= winner.culture and cu.id ~= loser.culture then stranger = cu.id end
end

-- spec: lore: Told from a side
do
  local ours = lore.war_account(db, war.id, winner.culture)
  local theirs = lore.war_account(db, war.id, loser.culture)
  if winner.culture == loser.culture then -- (a civil war: both tell it as the winners' people)
    expect_eq(ours.side, "winner")
  else
    expect(ours.side == "winner" and theirs.side == "loser", "both sides")
    expect(ours.title:find("Victory", 1, true) and theirs.title:find("Treachery", 1, true))
    expect(table.concat(ours.lines, " "):find("glorious victory", 1, true))
    expect(table.concat(theirs.lines, " "):find("treachery", 1, true))
    expect(ours.lines[2] ~= theirs.lines[2])
  end
  local plain = lore.war_account(db, war.id, stranger)
  expect(plain.side == "neutral" and #plain.lines == 2 and plain.lines[2]:find(winner.name, 1, true))
end

-- spec: lore: Books of kings
do
  local own = lore.book_of_kings(db, winner.id, winner.culture)
  expect(own[1] == "The Book of the Kings of " .. winner.name and #own >= 3)
  local ruler = history.get(db, winner.ruler)
  local found = false
  for _, line in ipairs(own) do
    if line:find(ruler.name, 1, true) then found = line:find(ruler.epithet, 1, true) ~= nil end
  end
  expect(found, "a realm's own book keeps its rulers' names")
  -- Told by the losers of a war against it: its good kings blackened.
  local hostile = lore.book_of_kings(db, winner.id, loser.culture)
  if winner.culture ~= loser.culture then
    local good = 0
    for _, dyn in ipairs(db.dynasty) do
      if dyn.polity == winner.id then
        for _, rid in ipairs(dyn.rulers) do
          local r = history.get(db, rid)
          for _, line in ipairs(hostile) do
            if line:find(r.name .. " " .. r.epithet, 1, true) and r.good then good = good + 1 end
          end
        end
      end
    end
    expect_eq(good, 0, "no good king of the enemy keeps his name")
  end
end

-- spec: lore: Legends drift
do
  expect(lore.drift(0, 0) == 0 and lore.drift(10000, 0) == 1 and lore.drift(100, 1000000) > lore.drift(100, 0))
  local near = lore.legend(db, war.battle, winner.culture, 1, 0)
  local far = lore.legend(db, war.battle, winner.culture, 900, 3000000)
  local function fallen(text) return tonumber(text:match("(%d+) fell")) end
  expect(near:find("In the year", 1, true) and far:find("days of old", 1, true))
  expect(fallen(far) > fallen(near) and far:find("gods themselves", 1, true))
  expect(lore.legend(db, war.id, winner.culture, 1, 0):find("fell where", 1, true))
  local quake = { year = 900, kind = "eruption" }
  history.event(db, quake)
  expect(lore.legend(db, quake.id, winner.culture, 1, 0):find("woke in fire", 1, true))
  expect(lore.legend(db, quake.id, winner.culture, 500, 0):find("was angered", 1, true))
  local rev = history.events(db, { kind = "revelation" })[1]
  expect(lore.legend(db, rev.id, winner.culture, 500, 0):find("pillar of light", 1, true))
  local other = history.events(db, { kind = "rise" })[1]
  expect(lore.legend(db, other.id, winner.culture, 1, 0):find(other.title, 1, true))
  expect(lore.legend(db, other.id, winner.culture, 900, 0):find("nothing was the same", 1, true))
end

-- spec: lore: Inscriptions in a people's script
do
  local kinds = {}
  for _, site in ipairs(db.site) do
    if not kinds[site.kind] then
      kinds[site.kind] = true
      local ins = lore.inscription(db, site.id)
      expect(ins.native ~= "" and ins.gloss ~= "" and ins.script, site.kind)
      expect(vfs.exists("fonts/script_" .. ins.script .. ".lua"), "a font for the script " .. ins.script)
      expect(ins.native:match("^[a-z ]+$"), "written in the script's letters: " .. ins.native)
      if site.kind == "elder ruin" then expect_eq(ins.language, db.old_tongue) end
    end
  end
  expect(kinds.tomb and kinds.battlefield and kinds["elder ruin"] and kinds.holy)
  local font = render.font("fonts/script_nastal.lua")
  expect(font and font > 0)
end

-- spec: lore: What people know
do
  local k = lore.knowledge(db, { culture = loser.culture, faith = db.faith[1].id, realm = loser.id })
  expect(#k.history >= 1 and #k.faith == 3)
  if winner.culture ~= loser.culture then expect(#k.grudges >= 1) end
  local w = lore.knowledge(db, { culture = winner.culture, realm = winner.id })
  expect(#w.grudges == 0 or winner.culture == loser.culture or w.grudges[1] ~= k.grudges[1])
  expect(#w.faith == 0)
  expect(#lore.knowledge(db, { culture = winner.culture }).history == 0)
end

-- spec: lore: The lore journal
do
  local j = lore.new()
  expect_eq(lore.learn(j, db, db.culture[1].id, "talk"), "Peoples")
  expect_eq(lore.learn(j, db, db.culture[1].id, "book"), nil)
  expect_eq(lore.learn(j, db, db.faith[1].id, "visit"), "Faiths")
  expect_eq(lore.learn(j, db, db.polity[1].id, "book"), "Realms")
  expect_eq(lore.learn(j, db, war.id, "book"), "History")
  expect_eq(lore.learn(j, db, "culture:999", "book"), nil)
  local counts = lore.counts(j, db)
  expect(counts.Peoples == 1 and counts.Faiths == 1 and counts.Realms == 1 and counts.History == 1)
  expect(lore.page(j, db, db.culture[1].id)[1]:find(db.culture[1].name, 1, true))
  expect(lore.page(j, db, db.faith[1].id)[1]:find(db.faith[1].deity, 1, true))
  expect(lore.page(j, db, db.polity[1].id)[1]:find(db.polity[1].name, 1, true))
  expect_eq(lore.page(j, db, war.id)[1], war.title)
  expect(lore.page(j, db, db.polity[2].id) == nil and lore.page(j, db, "nothing") == nil)
  local back = lore.new(lore.snapshot(j))
  expect_eq(lore.counts(back, db).Peoples, 1)
  expect(next(lore.new({ known = { [5] = "x" } }).known) == nil)
  j.known["culture:999"] = "talk" -- (a save from another world)
  expect(lore.page(j, db, "culture:999") == nil and lore.counts(j, db).Peoples == 1)
end
