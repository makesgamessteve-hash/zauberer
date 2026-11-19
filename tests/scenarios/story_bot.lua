-- SPDX-License-Identifier: MIT
-- The M10 exit demo: a story bot plays the whole story headlessly on the
-- engine's fixed seed - the prologue in Anarvan, the trail of ash, the
-- Circle's remnants and their revelations, a rival mage, divination,
-- readiness for the storm, the duel with Azhdar fought with real casts of
-- Light - and then, from the moment of choice, each of the four endings
-- with its world afterwards.
-- spec: narrative-director: The M10 exit demo
-- spec: story-act-3-and-endings: Every ending can be reached
-- spec: post-game-world-state: The world after the ending
scenario { name = "story bot" }
local game = demo.game
local places = demo.places
local prologue = require("story.prologue")
local saga = require("story.saga")
local narrative = require("story.narrative")
local underground = require("places.underground")
local pack = require("player.pack")
local wp = game.world_places
local P = wp.places
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if P.ready and world.chunk_stage(here) == 3 and sim.fire_at(here).loaded then break end
  sleep(5)
end
prologue.ARRIVAL, prologue.HIDING = 3, 5
local FOOT = { move = world.move, height = world.ground_height, water = world.water,
  structure = function(pos, alt) return places.floor(P, pos, alt) end,
  blocked = function(a, b, alt) return places.blocked(P, a, b, alt) end }
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
end
local function stand(pos, heading)
  game.walker = demo.walker.new(pos, heading or 0, FOOT)
  game.walker.pos.alt = pos.alt or world.height(pos)
  step(2)
end
local function walk_until(cond, seconds)
  for _ = 1, seconds * 60 do
    input.inject("steer", 0, 1, 1)
    step(1)
    if cond() then return true end
  end
  return false
end

-- The prologue (as Vertical Slice B), briskly.
press("story")
local S = game.story
local where = S.places
for _, p in ipairs({ where.qanat_pos, where.pen_pos, where.orchard }) do stand(p); press("use") end
local farrokh
for _, p in ipairs(where.plan.people) do if p.name == "Farrokh" then farrokh = p end end
stand(world.move(farrokh.pos, math.pi, 1, 0, 0), 0)
game.walker.pos.alt, farrokh.goal = farrokh.pos.alt, farrokh.pos
press("use")
for _ = 1, 60 * 4 do step(1) end
local g = where.sanctum
local top = world.move(g.origin, 0, g.stair.z0 - 0.3, g.stair.x, 0)
top.alt = g.origin.alt + underground.DEPTH
stand(top, math.pi)
walk_until(function() return S.beat == "hiding" end, 8)
for _ = 1, 60 * 7 do step(1) end
stand(top, math.pi)
walk_until(function() return S.beat == "sanctum" end, 8)
for _, p in ipairs(g.pieces) do
  if p.treasure then
    local at = world.move(g.origin, 0, p.z + 1.2, p.x, 0)
    at.alt = g.origin.alt
    stand(at, math.pi)
    press("use")
  end
end
press("sleep")
game.walker = nil
for _, r in ipairs(prologue.rings(S)) do game.carpet.pos = world.move(r, 0, 0, 0, 0); step(2) end
game.gathered_now = true
pack.add(game.pack, "boiled_water", 1, 0)
step(2)
game.carpet.pos = world.move(where.square, 0, 3200, 0, 0)
game.carpet.pos.alt = world.height(game.carpet.pos) + 60
step(3)
expect_eq(S.beat, "done", "the prologue is played through")

-- Act II begins: the director anchors the story; Azhdar's campaign burns the first place.
step(3)
local A = game.saga
expect(A and A.trail, "the trail of ash")
local N = A.N
for _, m in ipairs(narrative.data().circle) do expect(N.sites[m.id], "a site for " .. m.role) end
local function go(pos)
  game.carpet.pos = world.move(pos, 0, 0, 0, 0)
  game.carpet.pos.alt = world.height(pos) + 30
  stand(world.move(pos, 0, 0, 0, 0))
  step(3)
end
go(A.trail)
expect(N.done.trail, "the trail followed")
-- The Circle's remnants, nearest first, until three are found (and the truths told).
local order = {}
for _, m in ipairs(narrative.data().circle) do order[#order + 1] = m end
for _, m in ipairs(order) do
  if narrative.count(N.remnants) < 4 then go(N.sites[m.id].pos) end
end
expect(N.done.circle and narrative.count(N.revealed) >= 4 and N.done.truth, "the Circle's legacy and the truth")
expect(narrative.held(N) >= 3, "Weaves secured: " .. narrative.held(N))
-- A rival mage at their sanctum.
local mage = A.R.list[1]
go(require("people.history").get(P.db, mage.sanctum).pos)
expect(N.done.rivals and A.met[mage.id], "a rival mage met")
-- Act III: divination, then readiness for the storm.
saga.divine(A, world.time().time, game.disciplines.tier.ritual)
step(3)
expect(N.done.divination and A.citadel, "the citadel divined")
game.disciplines.tier.ritual = 3
local m = game.magic.caster.mage
for _, col in ipairs({ "water", "light", "storm", "void", "fire", "life", "earth", "decay" }) do m.mana[col] = 1000 end
pack.add(game.pack, "staff", 1, 0)
local ward = demo.artificing.enchant(demo.artificing.get("ash_ward"), { pack = game.pack, mage = m,
  artifacts = game.artifacts, factor = 1 })
expect(ward, "an ash ward made")
expect(demo.summoning.call(game.summoning, demo.summoning.get("wisp"), game.carpet.pos, m, 1), "an ally summoned")
for _ = 1, 30 do step(1) end
expect(N.done.storm, "ready for the storm")
-- The duel: Light in the right hand, cast again and again.
go(A.citadel)
expect(A.duel, "Azhdar faces the heirs")
press("wheel2")
for _ = 1, 400 do
  m.mana.light = 1000
  game.health.hp = game.health.max or 100
  input.inject("cast_right", 1, 0, 20)
  step(24)
  if A.duel.over then break end
end
expect(A.duel.won and N.defeated and N.done.duel, "Azhdar falls")
-- The endings, each from the moment of choice.
local snap = saga.snapshot(A)
local standing = {}
for k, v in pairs(wp.folk.economy.standing) do standing[k] = v end
local ley = mana.ley(game.carpet.pos).strength
for _, ending in ipairs({ "unravel", "bind", "reweave", "succession" }) do
  game.saga = saga.begin(saga.new(snap), P, world.time().time)
  A = game.saga
  for k in pairs(wp.folk.economy.standing) do wp.folk.economy.standing[k] = standing[k] end
  mana.set_ley_strength(ley)
  game.disciplines.tier.ritual = ending == "reweave" and 3 or 1
  if ending == "reweave" then -- (the Reweave needs Void as well as Ritual)
    require("magic.spellbook").learn(game.magic.caster.book, { verb = "unmake", colors = { "void" }, form = "tap",
      power = 1 }, "book")
  end
  step(2)
  local tries = 0
  local open = require("story.finale").endings({ ritual = game.disciplines.tier.ritual, void = ending == "reweave" })
  local want
  for i, id in ipairs(open) do if id == ending then want = i end end
  expect(want, ending .. " is open")
  while A.choice ~= want and tries < 8 do press("story"); tries = tries + 1 end
  press("use")
  expect_eq(A.ending, ending)
  expect(A.N.done.ending, "the story is complete: " .. ending)
  expect(A.last_chronicle, "a chronicle of it")
  local after = mana.ley(game.carpet.pos).strength
  if ending == "reweave" then expect_eq(after, 1) end
  if ending == "succession" then
    expect(wp.folk.economy.standing[P.db.polity[1].id] < (standing[P.db.polity[1].id] or 0), "tyrants")
  else
    expect(wp.folk.economy.standing[P.db.polity[1].id] > (standing[P.db.polity[1].id] or 0), "saviours")
  end
  if ending == "unravel" then -- magic fades year by year
    world.set_time(world.time().time + 400 * 86400)
    step(3)
    expect(mana.ley(game.carpet.pos).strength < after, "the ley fades after the Unravel")
  end
end
