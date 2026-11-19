-- SPDX-License-Identifier: MIT
-- The story's machinery in the peopled world: the director anchors the
-- Circle's remnants the same way every time, rumours point to them,
-- latecomers are met at a reunion; Azhdar's campaign burns, blights and
-- takes Weaves at a pace set by the heirs' progress; rival mages claim,
-- raid and trade; the saga is kept with the save.
scenario { name = "story" }
local places = demo.places
local narrative = require("story.narrative")
local campaign = require("story.campaign")
local rivals = require("story.rivals")
local saga = require("story.saga")
local history = require("people.history")
local game = demo.game
local P = game.world_places.places
for _ = 1, 600 do
  step(1)
  if P.ready then break end
end
local db, grid = P.db, P.grid
local home = history.get(db, db.story.anarvan).pos

-- spec: narrative-director: Beats anchored in the world
do
  local A, B = narrative.new(), narrative.new()
  narrative.anchor(A, db, grid, home)
  narrative.anchor(B, db, grid, home)
  local ids = {}
  for _, m in ipairs(narrative.data().circle) do
    local s = A.sites[m.id]
    expect(s and s.pos, "a site for " .. m.role)
    expect_eq(s.id, B.sites[m.id].id, "the same site from the same seed")
    expect(not ids[s.id], "each site reserved once")
    ids[s.id] = true
  end
  local tower = history.get(db, A.sites.hermit.id)
  expect(tower and tower.type, "the hermit in a tower or a hill place")
  expect(#narrative.candidates(db, grid, { kind = "grave" }) > 0)
  expect(#narrative.candidates(db, grid, { kind = "elder ruin" }) > 0)
  expect_eq(#narrative.open(A), 1, "the story opens with the trail of ash")
  expect_eq(narrative.open(A)[1].id, "trail")
end

-- spec: narrative-director: Rumours and reunions
do
  local N = narrative.new()
  narrative.anchor(N, db, grid, home)
  local r = narrative.rumour(N, home)
  expect(r and r:find("They say", 1, true) and r:find("to the", 1, true), tostring(r))
  for _, m in ipairs(narrative.data().circle) do N.remnants[m.id] = true end
  expect(narrative.rumour(N, home) == nil, "nothing left to point to")
  local st = narrative.reunion(N, db, 3, home)
  expect(st and not st.destroyed and N.reunions["3"].name == st.name, "a reunion at a place still standing")
  -- spec: story-act-2: The Circle's remnants
  local A = narrative.new()
  narrative.anchor(A, db, grid, home)
  local said = narrative.meet_remnant(A, "hermit", { learn = function(d) A.learnt = d end })
  expect(A.learnt == "summoning" and #said >= 2 and A.weaves.deep == "heirs")
  expect_eq(#narrative.meet_remnant(A, "hermit", {}), 0)
  A.weaves.dawn = "azhdar"
  said = narrative.meet_remnant(A, "astronomer", {})
  expect(said[#said]:find("Azhdar came first", 1, true), "a Weave already taken")
  local back = narrative.new(narrative.snapshot(A))
  expect(back.remnants.hermit and back.weaves.dawn == "azhdar")
end

-- spec: enemy-campaign: Azhdar's campaign
do
  local N = narrative.new()
  narrative.anchor(N, db, grid, home)
  local C = campaign.new()
  local origin = history.get(db, history.get(db, db.story.azhdar).home).pos
  campaign.plan(C, db, origin, N.sites, 0)
  expect(#C.targets > 10, "targets planned")
  local weaves = 0
  for _, t in ipairs(C.targets) do if t.circle then weaves = weaves + 1 end end
  expect_eq(weaves, 5, "every Circle site is a target")
  expect(campaign.tick(C, db, N, 0, 0) == nil, "not yet")
  local ley = 1
  local ctx = { set_ley = function(v) ley = v end, ley = 1 }
  local ev = campaign.tick(C, db, N, campaign.INTERVAL, 0, ctx)
  expect(ev and ev.kind == "burnt", "the first strike")
  local st = history.get(db, ev.settlement)
  expect(st.destroyed and st.fall_cause == "the Ashen" and campaign.blight_at(C, st.cell) > 0)
  expect(#C.refugees == 1 and ley < 1, "refugees on the roads, the ley weaker")
  -- The further the heirs, the sooner the next.
  local next_slow = C.next
  C.next = campaign.INTERVAL
  campaign.tick(C, db, N, campaign.INTERVAL, 4, ctx)
  expect(C.next - campaign.INTERVAL < next_slow - campaign.INTERVAL, "progress hastens him")
  -- He takes the Weaves the heirs have not secured, and is foiled where they have.
  -- spec: story-act-2: The Great Weaves
  N.weaves.verdant = "heirs"
  local taken, foiled = 0, 0
  for i = 1, 40 do
    local e = campaign.tick(C, db, N, campaign.INTERVAL * (i + 2) * 3, 0, ctx)
    if e and e.kind == "weave" then taken = taken + 1 end
    if e and e.kind == "foiled" then foiled = foiled + 1 end
  end
  expect(taken == 4 and foiled == 1, "four taken, one foiled: " .. taken .. " " .. foiled)
  expect(N.weaves.verdant == "heirs" and #C.taken == 4)
  expect(#campaign.rumours(C, db) >= 1)
  local back = campaign.new(campaign.snapshot(C))
  expect(#back.struck == #C.struck)
  for _, id in ipairs(C.struck) do -- (put the world back for the tests after)
    local s2 = history.get(db, id)
    s2.destroyed, s2.ruined = nil, nil
  end
end

-- spec: rival-mages-and-domains: Rival mages in the world
do
  local R = rivals.generate(db, narrative.data())
  local lieutenants = 0
  for _, m in ipairs(R.list) do
    expect(m.name and m.personality and #m.colours == 2 and m.sanctum and m.servants and m.familiar, m.id)
    if m.serves_azhdar then lieutenants = lieutenants + 1 end
  end
  expect_eq(lieutenants, 3, "Azhdar's three lieutenants")
  local cells = 0
  for _, m in ipairs(R.list) do for _ in pairs(m.domain) do cells = cells + 1 end end
  local events = rivals.tick(R, db, grid, 60)
  local kinds = {}
  for _, e in ipairs(events) do kinds[e.kind] = true end
  local after = 0
  for _, m in ipairs(R.list) do for _ in pairs(m.domain) do after = after + 1 end end
  expect(after > cells and kinds.claim, "domains grow")
  expect(kinds.trade or kinds.raid, "they trade or raid")
end

-- spec: narrative-director: The saga in the game
do
  local A = saga.begin(saga.new(), P, world.time().time)
  expect(A.trail and #A.C.struck == 1 and A.R and A.nexus, "begun: the first burning, the mages, the nexus")
  local lines = saga.hud_lines(A, home)
  expect(lines[1]:find("Story:", 1, true))
  local done = saga.tick(A, { P = P, pos = A.trail, now = world.time().time, economy = game.world_places.folk.economy })
  expect(done[1] == "trail", "the trail followed")
  local why = select(2, saga.divine(A, 0, 1))
  expect(why == nil and A.lines[#A.lines]:find("hold three Weaves", 1, true))
  local back = saga.begin(saga.new(saga.snapshot(A)), P, world.time().time)
  expect(back.N.done.trail and #back.C.struck == 1, "kept with the save")
  for _, id in ipairs(A.C.struck) do
    local s2 = history.get(db, id)
    s2.destroyed, s2.ruined = nil, nil
  end
  local _ = places
end
