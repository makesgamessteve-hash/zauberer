-- SPDX-License-Identifier: MIT
-- The Rise and fall variant: societies simulated over thousands of years
-- into the same history database - realms risen and fallen, wars fought
-- and won, cities sacked, plagues and famines, ruins with their causes -
-- the same from the same seed, kept with the save and taken back as it is.
scenario { name = "rise and fall" }
local history = require("people.history")
local chronicle = require("people.chronicle")
local simulation = require("people.simulation")
local lore = require("people.lore")
local job = require("people.job")
local play = require("places.play")

local anchor = world.position(0, 6506, 6025, 128, 128, 0)
local j = job.new(chronicle.settled_world, 4242, { variant = "rise", anchor = anchor })
local slices = 0
while not job.run(j, 1) do slices = slices + 1 end
local db = j.result

-- spec: history-simulation: Societies rise and fall
do
  expect_eq(db.variant, "rise and fall")
  local alive, fallen = 0, 0
  for _, p in ipairs(db.polity) do
    if p.fallen then fallen = fallen + 1 else alive = alive + 1; expect(p.capital, "a living realm has a capital") end
  end
  expect(alive >= 3 and fallen >= 1, "realms living and fallen: " .. alive .. " " .. fallen)
  local kinds = {}
  for _, e in ipairs(db.events) do kinds[e.kind] = true end
  expect(kinds.rise and kinds.collapse and kinds.war and kinds.battle and kinds["great work"], "the shape of history")
  expect(#db.wars > 10)
  for _, wid in ipairs(db.wars) do
    local war = history.get(db, wid)
    local w, l = history.get(db, war.winner), history.get(db, war.loser)
    expect(w and l and history.get(db, war.site).kind == "battlefield")
    local grudge = false
    for _, g in ipairs(l.grudges or {}) do grudge = grudge or g.war == wid end
    expect(grudge)
  end
  -- Ruins are left by what the simulation did to their land.
  for _, st in ipairs(db.settlement) do
    if st.ruined then expect(db.cell_ruin[st.cell] or st.fall_cause, "a cause") end
  end
  expect(simulation.capacity({ land = true, class = "grassland", fertility = 1 }) > 1000)
  expect_eq(simulation.capacity({ land = false }), 0)
  -- The same lore is told of it as of a Settled world.
  local war = history.get(db, db.wars[1])
  expect(#lore.war_account(db, war.id, history.get(db, war.winner).culture).lines >= 2)
  expect(slices > 20, "generation in slices")
end

-- spec: history-simulation: The same history from the same seed
do
  local again = chronicle.settled_world(4242, { variant = "rise", anchor = anchor })
  expect(#again.events == #db.events and #again.polity == #db.polity and #again.wars == #db.wars)
  expect_eq(again.events[#again.events].title, db.events[#db.events].title)
end

-- spec: history-simulation: Kept with the save
do
  local saved = history.save(db)
  expect(saved.by_id == nil and saved.cell_realm and saved.wars)
  local back = history.load(saved)
  expect(back and #back.polity == #db.polity
    and history.get(back, db.wars[1]).title == history.get(db, db.wars[1]).title)
  expect_eq(back.cell_realm, db.cell_realm)
  expect(history.load({ year = "x" }) == nil)
  -- In play: a new world of Rise and fall keeps its history in the save and takes it back.
  local s = play.setup(nil, 4242, anchor, nil, "rise")
  require("places.places").finish(s.places)
  local snap = play.snapshot(s)
  expect(snap.variant == "rise" and snap.history and snap.history.variant == "rise and fall")
  local s2 = play.setup(snap, 4242, anchor, nil, nil)
  require("places.places").finish(s2.places)
  expect(s2.variant == "rise" and #s2.places.db.polity == #s.places.db.polity)
  expect(s.places.db.sim and s2.places.db.sim == nil and s2.places.job.slices < s.places.job.slices,
    "taken back, not made again")
  local plain = play.snapshot(play.setup(nil, 4242, anchor, nil, nil))
  expect(plain.variant == "settled" and plain.history == nil)
end
