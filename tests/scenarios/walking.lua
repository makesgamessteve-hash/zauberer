-- SPDX-License-Identifier: MIT
-- On foot: the carpet comes down slow and low and the mage steps off; walks,
-- runs, turns and jumps; the creatures know the player is on foot; open water
-- too deep to wade stops the mage at the shore, and frozen it carries the
-- mage across; stepping back onto the carpet flies on.
-- spec: walking: Stepping off and on
-- spec: walking: Walking, running and jumping
-- spec: walking: Ice holds and deep water stops
scenario { name = "walking" }
local game = demo.game
local walker = demo.walker
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.water_at(here).loaded then break end
  sleep(5)
end
-- High and fast: no stepping off.
game.carpet.speed = 20
input.inject("mount", 1, 0, 1)
step(2)
expect(game.walker == nil, "too fast to step off")
-- Down low and slow: the mage steps off beside the carpet.
game.carpet.speed = 0
local c = game.carpet
c.pos.alt = world.ground_height(c.pos) + 1
step(1)
input.inject("mount", 1, 0, 1)
step(2)
local w = game.walker
expect(w ~= nil and w.grounded, "on foot")
expect(math.abs(w.pos.alt - world.ground_height(w.pos)) < 0.05, "on the ground")
expect(walker.near_carpet(w, game.carpet))
-- Walking forward, then running, both on the ground.
local start = w.pos
input.inject("steer", 0, 1, 120)
step(120)
local walked = world.distance(start, w.pos)
expect(walked > 2 and walked < 3.4, "walked " .. walked .. " m in 2 s")
start = w.pos
input.inject("steer", 0, 1, 60)
input.inject("boost", 1, 0, 60)
step(60)
local ran = world.distance(start, w.pos)
expect(ran > 3.5 and ran < 5.2, "ran " .. ran .. " m in 1 s")
expect(math.abs(w.pos.alt - world.ground_height(w.pos)) < 0.7, "following the ground")
-- Turning and jumping.
local heading = w.heading
input.inject("steer", 1, 0, 30)
step(30)
expect(math.abs(w.heading - heading) > 0.5, "turned")
input.inject("jump", 1, 0, 1)
step(10)
expect(not w.grounded and w.pos.alt > world.ground_height(w.pos) + 0.3, "in the air")
step(60)
expect(w.grounded, "landed")
-- The creatures know the player is on foot: the HUD shows the health too.
local lines = demo.flight_lines()
local shown = false
for _, l in ipairs(lines) do shown = shown or l:find("On foot", 1, true) ~= nil end
expect(shown, "on foot on the HUD")
-- Deep water stops the mage at the shore; frozen, it holds.
local ahead = world.move(w.pos, w.heading, 9, 0)
expect(world.deform(ahead, 6, -3) > 0, "a pit dug in front")
sim.add_water(ahead, 5, 150) -- filled to a pool too deep to wade
for _ = 1, 30 do step(10) end
expect(sim.water_at(ahead).depth > walker.WADE, "deep water ahead: " .. sim.water_at(ahead).depth)
input.inject("steer", 0, 1, 240)
step(240)
local _, depth = walker.floor(w.pos, { height = world.ground_height, water = world.water })
expect(w.blocked or depth <= walker.WADE, "stopped at the shore")
expect(depth <= walker.WADE)
sim.freeze(ahead, 6, 600)
input.inject("steer", 0, 1, 180)
step(180)
expect(w.on_ice, "standing on the ice")
expect(math.abs(w.pos.alt - sim.water_at(w.pos).level) < 0.1, "at the ice's level")
-- Back to the carpet and away.
game.walker.pos = world.move(game.carpet.pos, 0, 1, 0)
game.walker.pos.alt = game.carpet.pos.alt
input.inject("mount", 1, 0, 1)
step(2)
expect(game.walker == nil, "back on the carpet")
input.inject("climb", 1, 0, 60)
step(60)
expect(game.carpet.pos.alt > world.ground_height(game.carpet.pos) + 3, "flying again")
-- With a gamepad the right stick turns and tilts the view.
local w2 = walker.new(game.carpet.pos, 0, { move = world.move, height = world.ground_height, water = world.water })
walker.update(w2, { look = { x = 1, y = 1 }, mouse = false }, 0.5,
  { move = world.move, height = world.ground_height, water = world.water })
expect(w2.heading > 1 and w2.pitch > 0, "the stick turns and tilts (up looks up)")
local snap = walker.snapshot(w2)
expect(snap.pos and snap.heading == w2.heading)
-- Walking off an edge: the mage falls and lands.
local cliff = walker.new(game.carpet.pos, 0, { move = world.move, height = world.ground_height, water = world.water })
local env = { move = world.move, height = function(p) return world.ground_height(p) end, water = world.water }
cliff.pos.alt = cliff.pos.alt + 3 -- a ledge 3 m up
walker.update(cliff, { throttle = 1 }, 1 / 60, env)
expect(not cliff.grounded, "off the edge")
for _ = 1, 120 do walker.update(cliff, {}, 1 / 60, env) end
expect(cliff.grounded and cliff.landing == nil, "landed")
