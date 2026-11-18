-- SPDX-License-Identifier: MIT
-- Weaves through the simulation: a mage by the carpet casts the named weaves
-- of the base game and the world answers: fire burns and goes out, water
-- condenses, dries and freezes, the ground rises and falls, stones are
-- conjured and pushed, bolts fly and land, lights and wards come and go.
-- spec: weave-grammar: Weaves through the simulation
-- spec: weave-grammar: Casting and its refusals
scenario { name = "weaves" }
local cast = require("magic.cast")
local mage = require("magic.mage")
local spellbook = require("magic.spellbook")
local effects = require("magic.effects")
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded and world.chunk_stage(here) == 3 then break end
  sleep(5)
end
sim.set_weather({ wind_east = 0, wind_north = 0, rain = 0, snow = 0, cloud = 0, storm = 0 })
-- Dry, level ground with fuel near the carpet.
local spot
for r = 0, 120, 6 do
  for a = 0, 5.9, 0.785 do
    local p = world.move(here, a, r, 0)
    local c, wc = sim.fire_at(p), sim.water_at(p)
    local flat = math.abs(world.ground_height(world.move(p, 0, 12, 0)) - world.ground_height(p)) < 2
    if not spot and c.loaded and not c.water and c.fuel > 0.2 and c.moisture < 0.25 and wc.depth == 0 and flat then
      spot = p
    end
  end
end
expect(spot, "dry ground near the carpet")
local function above(p, h)
  local q = world.move(p, 0, 0, 0)
  q.alt = world.ground_height(q) + h
  return q
end
local full = {}
for _, c in ipairs(mage.COLORS) do full[c] = 120 end
local caster = cast.new(mage.new({ capacity = 2000, channel = 100, mana = full }))
local function refill()
  for _, c in ipairs(mage.COLORS) do caster.mage.mana[c] = 400 end
  caster.mage.strain = 0
end
-- Bolts fly north at a place from 10 m south of it; other weaves are cast
-- from above it, straight down.
local function weave_at(target, verb, colors, form, power, mods, opts)
  refill()
  local from = form == "line" and above(world.move(target, math.pi, 10, 0), 1.5) or above(target, 4)
  local pitch = form == "line" and 0 or -math.pi / 2 + 1e-3
  local w = { verb = verb, colors = colors, form = form, power = power, modifiers = mods }
  -- Unfamiliar weaves misfire now and then: cast again until one goes right
  -- (the misfires are tested on their own below).
  local first = cast.weave(caster, w, from, 0, pitch, opts or { accuracy = 1 })
  local r, all = first, { first }
  for _ = 1, 8 do
    if not (r.plan and r.plan.misfire) or (opts and opts.accuracy == 0) then break end
    refill()
    r = cast.weave(caster, w, from, 0, pitch, opts or { accuracy = 1 })
    all[#all + 1] = r
  end
  r.discovered, r.all = first.discovered, all
  return r
end
local function run(seconds)
  local happened = {}
  for _ = 1, math.floor(seconds * 60) do
    step(1)
    for _, h in ipairs(cast.tick(caster, 1 / 60, mana.at(here).stored)) do happened[#happened + 1] = h end
  end
  return happened
end
local function burning_near(p, r)
  local n = 0
  for dx = -r, r, 2 do
    for dy = -r, r, 2 do
      if sim.fire_at(world.move(p, math.atan(dx, dy), math.sqrt(dx * dx + dy * dy), 0)).burning then n = n + 1 end
    end
  end
  return n
end

-- Fire: a bloom of flame, then Unmake Fire puts it out.
local r = weave_at(spot, "create", { "fire" }, "circle", 30)
expect(r.ok and r.name == "Fire Bloom" and r.discovered, r.message)
expect(r.cost.total > 0 and r.effect.heat == 30 * 1000000)
run(3)
expect(burning_near(spot, 6) > 0, "it burns")
-- A number in the effects of all the attempts a cast took.
local function total(result, field)
  local sum = 0
  for _, attempt in ipairs(result.all) do sum = sum + (attempt.effect[field] or 0) end
  return sum
end
r = weave_at(spot, "unmake", { "fire" }, "circle", 60, { "wide" })
local put_out = total(r, "put_out")
expect(r.ok and r.name == "Unmake Fire" and put_out > 0, "fires put out: " .. put_out)
run(1)
expect_eq(burning_near(spot, 6), 0, "out")
-- A fireball flies and sets fire where it lands.
local far = world.move(spot, 0, 25, 0)
r = weave_at(far, "move", { "fire" }, "line", 30)
expect(r.ok and r.name == "Fireball" and r.effect.launched, r.message)
local landed = run(3)
expect(#landed >= 1 and landed[1].heat, "the fireball landed")
expect_eq(#caster.effects.projectiles, 0)
sim.cool(far, 30, 1e9)
-- Water: condensed, dried, frozen and thawed.
local pond = world.move(spot, 1.6, 30, 0)
sim.add_water(pond, 6, 60)
run(2)
r = weave_at(pond, "create", { "water" }, "circle", 50)
expect(r.ok and r.name == "Condense" and r.effect.water > 0)
r = weave_at(pond, "shape", { "water" }, "circle", 20)
expect(r.ok and r.name == "Freeze" and r.effect.frozen > 0, "frozen: " .. tostring(r.effect.frozen))
expect(sim.water_at(pond).frozen, "ice")
r = weave_at(pond, "shape", { "water" }, "circle", 20)
expect(r.effect.thawed > 0 and not sim.water_at(pond).frozen, "thawed")
r = weave_at(pond, "unmake", { "water" }, "circle", 50, { "wide" })
expect(r.ok and r.name == "Dry" and r.effect.dried > 0)
expect(sim.water_stats().removed > 0)
r = weave_at(world.move(pond, 0, 10, 0), "move", { "water" }, "line", 20)
expect(r.name == "Water Jet" and r.effect.launched)
landed = run(3)
expect(#landed >= 1 and landed[1].water, "the jet landed")
r = weave_at(pond, "sense", { "water" }, "circle", 2)
expect(r.name == "Dowse" and r.effect.sensed == "water" and r.effect.distance)
-- Earth: a wall, a pit, a conjured stone pushed by a gust, a hurled stone, mud.
local field = world.move(spot, 3.1, 40, 0)
local before = world.ground_height(field)
r = weave_at(field, "move", { "earth" }, "square", 20)
expect(r.name == "Raise Earth" and r.effect.chunks >= 1)
expect(world.ground_height(field) > before + 2, "a wall rose")
r = weave_at(field, "unmake", { "earth" }, "circle", 30)
expect(r.name == "Dig" and r.effect.chunks >= 1)
expect(world.ground_height(field) < before + 1, "dug down")
local yard = world.move(spot, 4.7, 40, 0)
r = weave_at(yard, "create", { "earth" }, "tap", 30)
expect(r.name == "Conjure Stone" and r.effect.stone, r.message)
expect(r.cost.total > weave_at(yard, "create", { "fire" }, "tap", 30).cost.total * 2, "matter from nothing is dear")
run(3)
local stone_id = r.effect.stone
local stone = sim.body(stone_id)
r = weave_at(stone.pos, "move", { "storm" }, "line", 20)
expect(r.name == "Gust" and r.effect.pushed >= 1)
run(0.5)
expect(world.distance(sim.body(stone_id).pos, stone.pos) > 0.2, "the gust moved the stone")
r = weave_at(world.move(yard, 0, 20, 0), "move", { "earth" }, "line", 10)
expect(r.name == "Hurl Stone" and r.effect.launched)
landed = run(3)
expect(#landed >= 1 and landed[1].struck)
r = weave_at(yard, "shape", { "earth", "water" }, "circle", 10)
expect(r.name == "Stone to Mud" and r.effect.mud > 0)
expect_eq(world.ground_surface(yard), "mud")
-- Light, life, void, decay and the senses.
r = weave_at(spot, "create", { "light" }, "tap", 5)
expect(r.name == "Light" and r.effect.light > 0 and #caster.effects.lights == 1)
r = weave_at(spot, "unmake", { "void" }, "circle", 5)
expect(total(r, "dispelled") >= 1 and #caster.effects.lights == 0, "dispelled")
weave_at(spot, "create", { "light" }, "tap", 1)
run(3)
expect_eq(#caster.effects.lights, 0, "the light faded")
r = weave_at(spot, "create", { "life" }, "tap", 5)
expect(r.name == "Mend" and r.effect.healed == 20)
local heard = effects.take_events(caster.effects)
expect(#heard == 1 and heard[1].kind == "heal")
r = weave_at(spot, "sense", { "life" }, "circle", 5)
expect(r.name == "Detect Life" and r.effect.plants >= 0)
r = weave_at(spot, "unmake", { "life" }, "line", 5)
expect(r.name == "Wither" and r.effect.withered >= 0)
r = weave_at(spot, "sense", { "void" }, "circle", 2)
expect(r.name == "Sense Void" and r.effect.line_distance >= 0)
r = weave_at(spot, "sense", { "fire" }, "circle", 2)
expect(r.name == "Heat Sight" and r.effect.temperature > -50)
r = weave_at(spot, "sense", { "storm" }, "circle", 2)
expect(r.name == "Read the Wind" and r.effect.sensed == "wind")
r = weave_at(spot, "create", { "decay" }, "circle", 5)
expect(r.name == "Rot" and r.effect.rotted >= 0)
r = weave_at(spot, "unmake", { "decay" }, "tap", 5)
expect(r.name == "Purify" and r.effect.purified == 5)
r = weave_at(spot, "create", { "storm" }, "tap", 2)
expect(r.name == "Spark")
r = weave_at(spot, "shape", { "fire" }, "tap", 2)
expect(r.effect.heat > 0)
sim.cool(spot, 20, 1e9)
for _, kind in ipairs({ "fire", "water", "storm", "earth" }) do
  local colors = { kind }
  r = weave_at(spot, "ward", colors, "tap", 1)
  expect(r.effect.ward == kind and effects.warded(caster.effects, kind), kind .. " ward up")
end
run(4)
expect(not effects.warded(caster.effects, "fire"), "wards fade")
local carpet = { pos = above(spot, 20) }
local start = carpet.pos
r = weave_at(spot, "move", { "void" }, "line", 5, nil, { accuracy = 1, carpet = carpet })
expect(r.name == "Blink" and r.effect.blinked == 10)
expect_near(world.distance(start, carpet.pos), 10, 0.5)
expect_eq(weave_at(spot, "move", { "void" }, "line", 5).effect.fizzled, "nothing to move")
-- Two colours: steam, healing rain, a thunderball.
r = weave_at(spot, "move", { "fire", "water" }, "triangle", 10)
expect(r.name == "Steam Blast" and r.effect.steam == 5)
r = weave_at(spot, "create", { "water", "life" }, "circle", 10)
expect(r.name == "Healing Rain" and r.effect.water > 0 and r.effect.healed > 0)
r = weave_at(far, "move", { "storm", "fire" }, "line", 10)
expect(r.name == "Thunderball" and r.effect.launched)
landed = run(3)
expect(#landed >= 1 and landed[1].heat, "the thunderball burst")
sim.cool(far, 30, 1e9)
r = weave_at(spot, "create", { "fire", "storm" }, "circle", 5)
expect(r.effect.heat and r.effect.sparked ~= nil, "each colour its share")
-- Weaves the world does not answer yet, and delays.
r = weave_at(spot, "bind", { "water" }, "circle", 5)
expect(r.ok and r.effect.fizzled == "the weave finds no hold")
r = weave_at(spot, "create", { "light" }, "tap", 1, { "delayed" })
expect(r.effect.delayed == 3 and #caster.effects.lights == 0)
local later = run(3.5)
expect(#later >= 1 and later[1].light, "the delayed light came")
-- Refusals, strain and misfires.
local w = { verb = "create", colors = { "fire" }, form = "circle", power = 10 }
local poor = cast.new(mage.new())
r = cast.weave(poor, w, above(spot, 2), 0, -0.3)
expect(not r.ok and r.message == "not enough mana" and r.cost.total > 0)
r = cast.weave(poor, { verb = "boil" }, above(spot, 2), 0, 0)
expect(not r.ok and r.message:find("no such verb"))
caster.mage.strain = 95
r = cast.weave(caster, w, above(spot, 2), 0, -0.3)
expect(not r.ok and r.message == "too strained to weave")
local misfires = 0
for k = 1, 30 do
  local m = weave_at(world.move(spot, 0, 60, 0), "create", { "fire", "water" }, "circle", 2, nil,
    { accuracy = 0, seed = k })
  if m.plan.misfire then
    misfires = misfires + 1
    expect(m.message:find("misfired"), m.message)
  end
end
expect(misfires > 3, "sloppy, volatile weaves misfire: " .. misfires)
local overdone = weave_at(spot, "create", { "light" }, "tap", 200, nil, { accuracy = 1, charge = 0.5 })
expect(overdone.ok and caster.mage.strain > 20, "overcharging strains")
local journal = spellbook.list(caster.book)
expect(#journal >= 30, "a full journal: " .. #journal)
expect(#spellbook.take_news(caster.book) >= 30)
