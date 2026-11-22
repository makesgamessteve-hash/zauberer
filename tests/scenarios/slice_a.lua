-- SPDX-License-Identifier: MIT
-- Vertical Slice A (M4), played through: fly out; scare a wolf pack away with
-- fire; the grass around catches; Unmake Fire puts it out; a Fireball brings
-- the storm bird down and the carpet collects its motes; at the water (a lake
-- in a created world, the sea in the first one) the mage steps off, finds it
-- too deep to wade, freezes it and walks out onto the ice; then flies on.
-- spec: vertical-slice: Vertical Slice A
scenario { name = "vertical slice a" }
local game = demo.game
local walker = demo.walker
local spellbook = require("magic.spellbook")
local weave = require("magic.weave")
local FOOT = { move = world.move, height = world.ground_height, water = world.water }

local function wait_loaded(pos)
  for _ = 1, 1200 do
    step(1)
    if sim.water_at(pos).loaded and sim.fire_at(pos).loaded and mana.at(pos).loaded then return true end
    sleep(5)
  end
  return false
end

-- Puts the carpet at a place (a scripted flight) and waits for the world there.
local function fly_to(pos, heading, height)
  local c = game.carpet
  c.pos = world.move(pos, 0, 0, 0)
  c.pos.alt = world.ground_height(c.pos) + (height or 30)
  c.heading, c.speed = heading or c.heading, 0
  expect(wait_loaded(c.pos), "the world streams in")
end

-- Casts a hand's weave, charged for some ticks (on foot, looking down at
-- pitch; from the carpet, along its heading a little down).
local function cast(hand, ticks, pitch)
  if game.walker and pitch then game.walker.pitch = pitch end
  input.inject("cast_" .. hand, 1, 0, ticks)
  step(ticks + 2)
  return game.last_cast
end

-- Puts a named weave, with modifiers, in the right hand: learnt from
-- Farrokh's books, prepared and assigned.
local function ready(name, modifiers)
  local book = game.magic.caster.book
  for _, w in ipairs(weave.data().named) do
    if w.name == name then
      local spec = { verb = w.verb, colors = w.colors, form = w.form, modifiers = modifiers or {} }
      spellbook.learn(book, spec, "book")
      local i = assert(spellbook.prepare(book, spec))
      assert(spellbook.assign(book, "right", i))
      return spec
    end
  end
  error("no weave " .. name)
end

local mage = game.magic.caster.mage
for c in pairs(mage.mana) do mage.mana[c] = 0 end
mage.mana.fire, mage.mana.water, mage.mana.storm = 60, 30, 0
expect(wait_loaded(game.carpet.pos))

-- 1. The wolves: fire on the ground between them and the carpet sends them off.
local wolves = {}
for _, id in ipairs(game.fauna.ids) do
  if sim.creature(id).species == "wolf" then wolves[#wolves + 1] = id end
end
expect_eq(#wolves, 3, "a pack of three")
local den = sim.creature(wolves[1]).pos
local toward = world.move(den, 0, 0, 0)
fly_to(world.move(den, 0, -25, 0), 0, 6)
local flare = world.move(den, 0, -8, 0)
local lit = sim.ignite(flare, 3) -- a Kindle at the grass in front of the pack
local r = cast("right", 30) -- and Kindle from the hand for good measure
expect(r and r.ok and r.name == "Kindle", r and r.message)
expect(lit > 0, "the grass caught")
step(60 * 2)
local fleeing = 0
for _, id in ipairs(wolves) do
  local w = sim.creature(id)
  fleeing = fleeing + ((w.action == "flee" or world.distance(w.pos, toward) > 15) and 1 or 0)
end
expect(fleeing >= 2, "the wolves run from the fire: " .. fleeing)
-- 2. The grass burns on: Unmake Fire puts it out.
step(60 * 4)
local burning = 0
for dx = -3, 3 do
  for dy = -3, 3 do
    burning = burning + (sim.fire_at(world.move(flare, 0, dx * 4, dy * 4)).burning and 1 or 0)
  end
end
expect(burning > 3, "the fire spreads: " .. burning .. " cells")
-- From 2 m up (the hands 3 m) and 25 m back the aim, 0.12 rad down, meets the
-- ground at the fire.
fly_to(world.move(flare, 0, -24.9, 0), 0, 2)
ready("Unmake Fire", { "wide", "wide" })
local function burning_now()
  local n = 0
  for dx = -3, 3 do
    for dy = -3, 3 do
      n = n + (sim.fire_at(world.move(flare, 0, dx * 4, dy * 4)).burning and 1 or 0)
    end
  end
  return n
end
local put_out = 0
for _ = 1, 6 do -- cast until it is out, mana topped up as a player would channel
  mage.mana.fire = 90
  r = cast("right", 90)
  if r and r.ok and r.effect then put_out = put_out + (r.effect.put_out or 0) end
  step(30)
  if burning_now() == 0 then break end
end
burning = burning_now()
expect(put_out > 0 and burning == 0, "put out: " .. put_out .. ", still burning: " .. burning)
-- 3. The storm bird: a Fireball brings it down, and its motes are collected.
local bird
for _, id in ipairs(game.fauna.ids) do
  if sim.creature(id).species == "storm_bird" then bird = id end
end
fly_to(sim.creature(bird).pos, 0, 0)
-- Keep 30 m off (closer, it takes fright), aim where the bird will be when the
-- bolt arrives, and hold the Fireball (the left hand) for a full second.
local function take_aim()
  local cr = sim.creature(bird)
  local d = 30
  local t = d / 40 + 0.05 -- the bolt's flight
  local ahead = world.move(cr.pos, cr.heading, cr.speed * t, 0)
  local c = game.carpet
  local from = world.move(ahead, math.pi + cr.heading, d, 0)
  c.pos = from
  local bp, cp = world.to_local(ahead), world.to_local(from)
  c.heading, c.speed = math.atan(bp[1] - cp[1], -(bp[3] - cp[3])), 0
  -- The bolt leaves 1 m above the carpet, 0.12 rad down, and falls.
  c.pos.alt = ahead.alt + d * math.tan(-game.rider.pitch) + 0.5 * 9.81 * t * t - 1
end
local shots = 0
for _ = 1, 3 do -- a miss is tried again
  mage.mana.fire = 40
  input.inject("cast_left", 1, 0, 60)
  for _ = 1, 62 do -- held for 60 ticks, cast on letting go
    take_aim()
    step(1)
  end
  shots = shots + 1
  r = game.last_cast
  expect(r and r.ok and r.name == "Fireball", r and r.message)
  for _ = 1, 60 do
    step(1)
    if not sim.creature(bird).alive then break end
  end
  if not sim.creature(bird).alive then break end
end
step(2)
expect(not sim.creature(bird).alive, "the storm bird fell")
expect(#game.motes.list >= 8, "its motes")
local fell = game.motes.list[#game.motes.list].pos -- the motes hang where it died; the bird drops
mage.mana.fire = 0 -- spent on the fire
fly_to(fell, 0, 0)
game.carpet.pos.alt = fell.alt
step(60 * 4)
expect(mage.mana.storm > 30, "storm mana collected: " .. mage.mana.storm)
local body = sim.creature(bird).pos
expect(math.abs(body.alt - world.ground_height(body)) < 0.1, "the bird lies on the ground")
-- 4. The lake: too deep to wade; frozen, it carries the mage.
local home = game.home.pos
-- The nearest deep water (the first world has the sea and no lakes; lakes
-- come with created worlds), searched in rings around home, and its shore:
-- back towards home until the ground is dry.
local shore, out_heading
for ring = 1, 200 do
  for k = 0, 31 do
    local h = k * math.pi / 16
    local kind, _, depth = world.water(world.move(home, h, ring * 100, 0))
    if kind and kind ~= "river" and depth > 2 then
      for metres = ring * 100, 0, -10 do
        local q = world.move(home, h, metres, 0)
        if not world.water(q) then
          shore, out_heading = q, h
          break
        end
      end
    end
    if shore then break end
  end
  if shore then break end
end
expect(shore ~= nil, "its shore")
-- Shallow flats may lie off the shore: start a few metres before the water
-- gets too deep to wade.
local heading = out_heading -- on towards the water
local edge
for metres = 0, 3000, 2 do
  local q = world.move(shore, heading, metres, 0)
  local kind, _, deep = world.water(q)
  if kind and deep > walker.WADE + 0.3 then
    edge = q
    break
  end
end
expect(edge ~= nil, "deep water off the shore")
local into = world.move(edge, heading + math.pi, 8, 0)
fly_to(into, heading, 1)
game.carpet.pos = world.move(into, heading + math.pi, 2, 0)
game.carpet.pos.alt = math.max(world.ground_height(game.carpet.pos), sim.water_at(game.carpet.pos).level) + 1
step(2)
input.inject("mount", 1, 0, 1)
step(3)
local w = game.walker
expect(w ~= nil, "on foot at the water")
w.heading = heading
input.inject("steer", 0, 1, 60 * 6) -- wade out until it gets too deep
step(60 * 6)
local _, depth = walker.floor(world.move(w.pos, w.heading, 1.5, 0), FOOT)
expect(depth > walker.WADE or w.blocked, "the water is too deep to wade")
local stopped = world.distance(w.pos, into)
-- Freeze: a wide circle on the water ahead.
ready("Freeze", { "wide" })
mage.mana.water = 60
for _ = 1, 4 do -- cast again until the water ahead freezes (a misfire may go astray)
  mage.mana.water = 60
  r = cast("right", 60, -0.5)
  if r and r.ok and (r.effect.frozen or 0) > 0 then break end
end
expect(r and r.ok and r.effect and (r.effect.frozen or 0) > 0, r and r.message)
input.inject("steer", 0, 1, 60 * 6)
input.inject("boost", 1, 0, 60 * 6)
step(60 * 6)
expect(w.on_ice, "standing on the ice")
expect(world.distance(w.pos, into) > stopped + 4, "out on the ice")
-- 5. Back to the carpet and on.
w.pos = world.move(game.carpet.pos, 0, 1, 0)
w.pos.alt = game.carpet.pos.alt
input.inject("mount", 1, 0, 1)
step(2)
expect(game.walker == nil, "back on the carpet")
input.inject("climb", 1, 0, 120)
input.inject("steer", 0, 1, 120)
step(120)
expect(game.carpet.pos.alt > world.ground_height(game.carpet.pos) + 5, "flying on")
expect(game.health.hp > 0, "alive at the end")
