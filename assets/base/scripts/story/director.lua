-- SPDX-License-Identifier: MIT
-- The story in the game: the prologue (story.prologue) begun in Anarvan,
-- fed each tick with what the heir does, the village set alight and laid
-- in ruins by it, the carpet held to the ground until it has chosen the
-- heir, Azhdar drawn on his dark carpet.
local prologue = require("story.prologue")
local places = require("places.places")
local history = require("people.history")
local buildings = require("places.buildings")

local director = {}

-- The places the prologue needs, from Anarvan's laid-out plan.
function director.places(P)
  local anarvan = places.find(P, history.get(P.db, P.db.story.anarvan).pos, "Anarvan")
  local plan = places.plan(P, anarvan)
  local house = plan.farrokh
  places.realise(P, house, 3)
  local loom
  for _, p in ipairs(house.pieces) do
    if p.item == "loom" and not loom then loom = places.to_world(P, house, p.x, 0, p.z) end
  end
  local orchard = world.move(anarvan.pos, math.pi, (plan.radius or 60) + 30, 0, 0)
  orchard.alt = world.height(orchard)
  local houses = {}
  for _, b in ipairs(plan.buildings) do
    if b.type ~= "pen" and b.type ~= "qanat_outlet" then houses[#houses + 1] = b end
  end
  return { plan = plan, settlement = anarvan, square = world.move(anarvan.pos, 0, 0, 0, 0),
    qanat_pos = places.origin(P, plan.qanat), pen_pos = places.origin(P, plan.pen),
    farrokh_pos = places.origin(P, house), loom_pos = loom or places.origin(P, house), orchard = orchard,
    sanctum = P.under.dungeons["sanctum:farrokh"], houses = houses, house = house }
end

-- Begins the prologue: the heir by Farrokh's door on a spring morning.
function director.start(P)
  local where = director.places(P)
  local S = prologue.new(where)
  local door
  for _, d in ipairs(where.house.doors) do if d.kind == "front" then door = d end end
  S.start = places.to_world(P, where.house, door and door.pos or 0, 0, where.house.d / 2 + 2)
  return S
end

-- Sets Anarvan alight and, after the night, lays it in ruins (its people gone).
function director.burn(P, b)
  local at = places.origin(P, b)
  sim.ignite(at, 3)
  b.burn_left = 300
end

function director.ruin(where)
  local st = where.settlement
  st.destroyed, st.population = true, 0
  for _, b in ipairs(where.plan.buildings) do
    b.ruin = { age = 1, cause = "fire", seed = b.seed }
    b.stage, b.pieces, b.rooms, b.doors = 1, nil, nil, nil
    buildings.generate(b, where.plan.family, 1)
  end
  where.plan.people = {}
end

-- One tick of the story. ctx as prologue.tick (pos, used, slept, dt, now,
-- set_time, talked_to, flying, book, pack, found, gathered, boiled).
function director.tick(S, P, ctx)
  ctx.burn = function(b) director.burn(P, b) end
  ctx.ruin = function() director.ruin(S.places) end
  return prologue.tick(S, ctx)
end

return director
