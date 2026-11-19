-- SPDX-License-Identifier: MIT
-- Vertical Slice B played through: the prologue in Anarvan (chores, a word
-- with Farrokh, Azhdar's arrival at dusk, the handover, hiding in the
-- warded cellar while the village burns, the ashes and the burial) and Act
-- I (the sanctum's letter, Primer and bag, the carpet choosing the heir in
-- a dream, the first flight through the rings, foraging and boiled water,
-- leaving the valley).
-- spec: vertical-slice: Vertical Slice B
scenario { name = "slice b" }
local game = demo.game
local places = demo.places
local prologue = require("story.prologue")
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
prologue.ARRIVAL, prologue.HIDING = 5, 10 -- (shorter waits for the test)
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
end
local function beat() return game.story and game.story.beat end
local FOOT = { move = world.move, height = world.ground_height, water = world.water,
  structure = function(pos, alt) return places.floor(P, pos, alt) end,
  blocked = function(a, b, alt) return places.blocked(P, a, b, alt) end }
local function stand(pos, heading)
  game.walker = demo.walker.new(pos, heading or 0, FOOT)
  game.walker.pos.alt = pos.alt or world.height(pos)
  step(2)
end

press("story")
expect(game.story and beat() == "village", "F5 begins the prologue")
expect(game.walker, "on foot by Farrokh's door")
local S = game.story
local where = S.places
expect(where.house.role == "farrokh" and where.sanctum and where.sanctum.closed, "Farrokh's house, its cellar hidden")
expect(game.carpet.limits.fly == false, "the carpet does not fly yet")

-- Chores.
stand(where.qanat_pos)
press("use")
stand(where.pen_pos)
press("use")
stand(where.orchard)
press("use")
expect(S.chores.water and S.chores.goats and S.chores.pomegranates, "water, goats, pomegranates")
expect(pack.count(game.pack, "pomegranate") >= 3)
-- A word with Farrokh.
local farrokh
for _, p in ipairs(where.plan.people) do if p.name == "Farrokh" then farrokh = p end end
expect(farrokh and farrokh.pos, "Farrokh is at home")
stand(world.move(farrokh.pos, math.pi, 1, 0, 0), 0)
game.walker.pos.alt = farrokh.pos.alt
farrokh.goal = farrokh.pos
press("use")
expect(S.talked, "talked with Farrokh")
expect_eq(beat(), "arrival")
expect(world.time().hour >= 18, "dusk")
for _ = 1, 60 * 6 do step(1) end
expect_eq(beat(), "handover")
expect(pack.count(game.pack, "rug") == 1 and not where.sanctum.closed, "the rug, the hatch open")
-- Down the stair into the cellar.
local g = where.sanctum
local top = world.move(g.origin, 0, g.stair.z0 - 0.3, g.stair.x, 0)
top.alt = g.origin.alt + underground.DEPTH
stand(top, math.pi)
for _ = 1, 60 * 8 do
  input.inject("steer", 0, 1, 1)
  step(1)
  if beat() == "hiding" then break end
end
expect_eq(beat(), "hiding")
expect(game.walker.pos.alt < world.ground_height(game.walker.pos) - 3, "in the cellar")
for _ = 1, 60 * 12 do step(1) end
expect_eq(beat(), "ashes")
expect(where.settlement.destroyed and #where.plan.people == 0, "Anarvan is ash")
for _, b in ipairs(where.plan.buildings) do expect(b.ruin and b.ruin.cause == "fire") end
-- The burial.
stand(where.farrokh_pos)
press("use")
expect(S.journal.burial, "Farrokh buried, a lamp lit")
-- Back down into the sanctum.
stand(top, math.pi)
for _ = 1, 60 * 8 do
  input.inject("steer", 0, 1, 1)
  step(1)
  if beat() == "sanctum" then break end
end
expect_eq(beat(), "sanctum")
for _, p in ipairs(g.pieces) do
  if p.treasure then
    local at = world.move(g.origin, 0, p.z + 1.2, p.x, 0)
    at.alt = g.origin.alt
    stand(at, math.pi)
    press("use")
  end
end
expect(S.journal.letter and S.primer and S.bag or beat() ~= "sanctum", "letter, Primer, bag")
expect_eq(beat(), "carpet")
local book = game.magic.caster.book
local names = {}
for _, e in ipairs(require("magic.spellbook").list(book)) do names[e.name] = true end
expect(next(names), "the Primer's weaves in the spellbook")
-- The carpet chooses the heir in a dream.
press("sleep")
expect_eq(beat(), "flying")
expect(prologue.can_fly(S) and game.carpet.limits.fly ~= false)
-- The first flight through the rings.
game.walker = nil
for _, r in ipairs(prologue.rings(S)) do
  game.carpet.pos = world.move(r, 0, 0, 0, 0)
  game.carpet.speed = 0
  step(2)
end
expect_eq(beat(), "survival")
-- Forage and boil water.
game.gathered_now = true
pack.add(game.pack, "boiled_water", 1, 0)
step(2)
expect_eq(beat(), "leaving")
-- Out of the valley.
game.carpet.pos = world.move(where.square, 0, 3200, 0, 0)
game.carpet.pos.alt = world.height(game.carpet.pos) + 60
step(2)
expect_eq(beat(), "done")
expect(#prologue.hud_lines(S) >= 2)
local snap = prologue.snapshot(S)
expect(snap.beat == "done" and snap.bonded and snap.journal.letter)
