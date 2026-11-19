-- SPDX-License-Identifier: MIT
-- The M8 exit demo, played through: fly into a generated oasis city and
-- enter a house; rest in a caravanserai; explore a citadel of more than a
-- hundred rooms; read two chronicles that tell the same war from opposite
-- sides; trade and get directions; descend into a karst cave with an
-- underground river.
-- spec: settlements: The M8 exit demo
scenario { name = "places and people" }
local game = demo.game
local places = demo.places
local folk = require("people.folk")
local history = require("people.history")
local underground = require("places.underground")
local caves = require("places.caves")
local land = require("people.land")
local pack = require("player.pack")
local wp = game.world_places
local P = wp.places
for _ = 1, 900 do
  step(1)
  if P.ready then break end
  sleep(5)
end
local db = P.db
local FOOT = { move = world.move, height = world.ground_height, water = world.water,
  structure = function(pos, alt) return places.floor(P, pos, alt) end,
  blocked = function(a, b, alt) return places.blocked(P, a, b, alt) end }
local function arrive(pos) -- fly in: the last kilometre on the carpet
  game.walker = nil
  local c = game.carpet
  c.pos = world.move(pos, 0, -1000, 0, 0)
  c.pos.alt = world.height(c.pos) + 80
  c.heading, c.speed = 0, 30
  for _ = 1, 900 do
    step(1)
    if world.distance(c.pos, pos) < 60 then break end
    c.heading = math.atan(world.to_local(pos)[1] - world.to_local(c.pos)[1],
      -(world.to_local(pos)[3] - world.to_local(c.pos)[3]))
    c.speed = 30
    sleep(2)
  end
  for _ = 1, 600 do
    step(1)
    if world.chunk_stage(c.pos) == 3 then break end
    sleep(5)
  end
  step(5)
end
local function on_foot(pos, heading)
  game.walker = demo.walker.new(pos, heading or 0, FOOT)
  game.walker.pos.alt = pos.alt
  step(2)
end

-- 1. Into an oasis city, into a house.
local oasis = places.find(P, game.carpet.pos, "oasis_town")
expect(oasis, "an oasis town in the world")
arrive(oasis.pos)
local plan = P.plans[oasis.id]
expect(plan and #plan.buildings >= 20, "laid out as the carpet comes")
local roles = {}
for _, b in ipairs(plan.buildings) do roles[b.type] = b end
expect(roles.qanat_outlet or roles.cistern, "its water")
expect(roles.bazaar and roles.caravanserai and roles.temple, "its bazaar, caravanserai and temple")
local house = roles.home or roles.weaver or roles.farmhouse
places.realise(P, house, 3)
local door
for _, d in ipairs(house.doors) do if d.kind == "front" then door = d end end
on_foot(places.to_world(P, house, door.pos, 0, house.d / 2 + 1.5), house.yaw + math.pi)
local b, piece = places.nearby(P, game.walker.pos, game.walker.heading)
if b == house and piece.kind == "door" then places.toggle_door(P, house, piece) end
for _ = 1, 60 * 3 do input.inject("steer", 0, 1, 1); step(1) end
expect(places.at(P, game.walker.pos) == house, "inside a house of the oasis")

-- 2. Trade and directions at the bazaar.
local merchant
for _, p in ipairs(plan.people or {}) do if p.profession == "merchant" then merchant = p end end
expect(merchant, "a merchant")
pack.add(game.pack, "coin", 200, 0)
local heir = { pack = game.pack, rest = function() end, heal = function() end, learn = function() end }
local bought = folk.serve(wp.folk, P, merchant, heir)
expect(bought:find("Bought", 1, true), bought)
local directions
for _ = 1, 10 do
  local line, _, topic = folk.talk(wp.folk, P, merchant, {})
  if topic == "directions" then directions = line end
end
expect(directions and directions:find("to the", 1, true), "directions: " .. tostring(directions))

-- 3. A night in a caravanserai.
local serai = places.find(P, oasis.pos, "caravanserai")
arrive(serai.pos)
local splan = P.plans[serai.id]
local keeper
for _, p in ipairs(splan.people or {}) do if p.profession == "innkeeper" then keeper = p end end
expect(keeper, "an innkeeper")
local hours = 0
heir.rest = function(h) hours = h end
expect(folk.serve(wp.folk, P, keeper, heir):find("rest the night", 1, true) and hours == 8)
local rooms = 0
for _, bb in ipairs(splan.buildings) do if bb.type == "caravanserai" then rooms = #bb.program end end
expect(rooms >= 20, "a caravanserai of " .. rooms .. " rooms")

-- 4. A citadel of more than a hundred rooms.
local city = places.find(P, game.carpet.pos, "city")
arrive(city.pos)
local cplan = P.plans[city.id]
expect(cplan.castle and cplan.castle.rooms >= 100, "a citadel of " .. (cplan.castle and cplan.castle.rooms or 0))
local palace
for _, bb in ipairs(cplan.buildings) do if bb.type == "palace" then palace = bb end end
places.realise(P, palace, 3)
local audience = require("places.buildings").rooms_of(palace, "audience")[1]
expect(audience and palace.iwan, "the audience hall with its iwan")
on_foot(places.to_world(P, palace, (audience.x0 + audience.x1) / 2, 0, (audience.z0 + audience.z1) / 2))
expect(places.at(P, game.walker.pos) == palace, "in the palace")

-- 5. Two chronicles of one war, from its two sides.
local war
for _, wid in ipairs(db.wars) do
  local ev = history.get(db, wid)
  if not war and history.get(db, ev.winner).culture ~= history.get(db, ev.loser).culture then war = ev end
end
expect(war, "a war between two peoples")
local function a_house_of(realm)
  for _, st in ipairs(db.settlement) do
    if st.realm == realm and not st.ruined then
      local pl = places.plan(P, st)
      for _, bb in ipairs(pl.buildings) do
        if bb.type == "home" or bb.type == "weaver" or bb.type == "farmhouse" then return bb end
      end
    end
  end
end
local wins = a_house_of(war.winner)
local loses = a_house_of(war.loser)
local placeplay = demo.placeplay
local t1, l1 = placeplay.read(wp, wins, war.id) -- (each house's chronicle of that war)
local t2, l2 = placeplay.read(wp, loses, war.id)
expect(t1 ~= t2 and l1[1] ~= l2[1], "the same war told two ways")
expect(t1:find("Victory", 1, true) and t2:find("Treachery", 1, true), t1 .. " / " .. t2)

-- 6. Down into a karst cave with an underground river.
local mouth
for _, c in ipairs(P.grid.cells) do
  if not mouth and c.land then
    for _, m in ipairs(underground.mouths(db, P.grid, c)) do
      if m.kind == "karst" and not mouth then mouth = m end
    end
  end
end
expect(mouth, "a karst cave in the world")
arrive(mouth.pos)
underground.tick(P.under, db, P.grid, { mouth.pos })
local cave = P.under.caves[mouth.seed]
expect(cave and cave.spring and cave.lake, "a karst cave with a river")
local c1 = cave.chambers[1]
on_foot(world.move(mouth.pos, math.atan(c1.x, c1.z), -0.5, 0, 0), math.atan(c1.x, c1.z))
for _ = 1, 60 * 30 do
  input.inject("steer", 0, 1, 1)
  step(1)
  local x, _, z = underground.local_of(cave.pos, game.walker.pos)
  if x * x + z * z > c1.x * c1.x + c1.z * c1.z then break end
end
expect(game.walker.pos.alt < world.ground_height(game.walker.pos) - 3, "under the ground")
local v = caves.water_volume(cave)
for _ = 1, 20 do caves.flow(cave, 0.5) end
expect(caves.water_volume(cave) ~= v, "the river runs")
local _ = land
