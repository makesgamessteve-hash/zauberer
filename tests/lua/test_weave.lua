-- SPDX-License-Identifier: MIT
-- Unit tests of the weave grammar (assets/base/scripts/magic/weave.lua).
local weave = require("magic.weave")

local function w(verb, colors, form, power, modifiers)
  return { verb = verb, colors = colors, form = form, power = power or 10, modifiers = modifiers }
end

-- spec: weave-grammar: Words, colours, forms and modifiers
test.case("the grammar is open and checked", function()
  local d = weave.data()
  local verbs = 0
  for _ in pairs(d.verbs) do verbs = verbs + 1 end
  test.expect_eq(verbs, 8)
  test.expect_eq(#d.colors, 8)
  test.expect(weave.validate(w("move", { "fire", "storm" }, "line")))
  test.expect(weave.validate(w("bind", { "decay" }, "wave", 1, { "lasting", "far" })))
  local cases = {
    { w("burn", { "fire" }, "line"), "no such verb" },
    { w("move", {}, "line"), "one or two colours" },
    { w("move", { "fire", "storm", "light" }, "line"), "one or two colours" },
    { w("move", { "plasma" }, "line"), "no such colour" },
    { w("move", { "fire", "fire" }, "line"), "two different colours" },
    { w("move", { "fire" }, "hexagon"), "no such form" },
    { w("move", { "fire" }, "line", 10, { "loud" }), "no such modifier" },
    { w("move", { "fire" }, "line", 0), "power must be" },
    { w("move", { "fire" }, "line", 5000), "power must be" },
  }
  for _, c in ipairs(cases) do
    local ok, err = weave.validate(c[1])
    test.expect(not ok and err:find(c[2], 1, true), tostring(err))
  end
  test.expect(not weave.validate("fireball"))
end)

test.case("relations on the wheel", function()
  test.expect_eq(weave.relation("fire", "water"), "opposite")
  test.expect_eq(weave.relation("storm", "earth"), "opposite")
  test.expect_eq(weave.relation("fire", "storm"), "neighbour")
  test.expect_eq(weave.relation("decay", "fire"), "neighbour") -- the wheel closes
  test.expect_eq(weave.relation("earth", "fire"), "other")
  test.expect_eq(weave.relation("life", "life"), "same")
end)

test.case("names, keys and words", function()
  test.expect_eq(weave.key(w("move", { "storm", "fire" }, "line")), "move:fire+storm:line")
  test.expect_eq(weave.name(w("move", { "storm", "fire" }, "line")), "Thunderball")
  local name, known = weave.name(w("unmake", { "fire" }, "circle"))
  test.expect_eq(name, "Unmake Fire")
  test.expect(known)
  name, known = weave.name(w("bind", { "void", "decay" }, "zigzag"))
  test.expect_eq(name, "Bind Void and Decay (zigzag)")
  test.expect(not known)
  test.expect_eq(weave.words(w("move", { "storm", "fire" }, "line", 10, { "far" })), "vash atash-vad dur")
end)

-- spec: weave-grammar: The cost of a weave
test.case("cost: form, modifiers, distance, place, efficiency, matter", function()
  local base = weave.cost(w("create", { "fire" }, "tap", 10)).total
  test.expect_near(base, 10 * 1.0 * 0.8 * 1.0, 1e-9) -- create x tap, an ordinary place
  local circle = weave.cost(w("create", { "fire" }, "circle", 10)).total
  test.expect(circle > base, "a wider form costs more")
  local far = weave.cost(w("create", { "fire" }, "tap", 10, { "far", "wide" })).total
  test.expect_near(far, base * 1.4 * 1.5, 1e-9)
  test.expect(weave.cost(w("create", { "fire" }, "tap", 10), { distance = 50 }).total > base)
  local rich = weave.cost(w("create", { "fire" }, "tap", 10), { place = { fire = 120 } }).total
  local thin = weave.cost(w("create", { "fire" }, "tap", 10), { place = { fire = 0 } }).total
  test.expect(rich < base and thin > base, "fire is cheap at a volcano, dear in a blizzard")
  test.expect_near(weave.cost(w("create", { "fire" }, "tap", 10), { efficiency = 0.3 }).total, base * 0.7, 1e-9)
  test.expect_near(weave.cost(w("create", { "fire" }, "tap", 10), { efficiency = 5 }).total, base * 0.4, 1e-9)
  local stone = weave.cost(w("create", { "earth" }, "tap", 10)).total
  test.expect_near(stone, base * 4, 1e-9) -- matter from nothing
  test.expect_near(weave.cost(w("create", { "earth" }, "tap", 10), { material = true }).total, base, 1e-9)
  local two = weave.cost(w("move", { "fire", "storm" }, "line", 10))
  test.expect_near(two.per.fire + two.per.storm, two.total, 1e-9)
  test.expect(two.per.fire > two.per.storm)
end)

-- spec: weave-grammar: Control and misfires
test.case("control, familiarity and misfires", function()
  test.expect_eq(weave.tier(0).name, "Unfamiliar")
  test.expect_eq(weave.tier(15).name, "Practised")
  test.expect_eq(weave.tier(40).name, "Known")
  test.expect_eq(weave.tier(500).name, "Mastered")
  test.expect(weave.control(1.0, 200) > weave.control(1.0, 0))
  test.expect(weave.control(0.6, 0) < weave.control(0.95, 0))
  test.expect(weave.control(1, 1000) <= 1)
  local fire = w("move", { "fire" }, "line")
  local calm = weave.misfire_chance(fire, 0.95, 0, 50)
  test.expect(weave.misfire_chance(fire, 0.5, 0, 50) > calm, "sloppy gestures misfire more")
  test.expect(weave.misfire_chance(w("move", { "fire", "water" }, "line"), 0.95, 0, 50) > calm + 0.15, "opposites")
  test.expect(weave.misfire_chance(w("move", { "fire", "storm" }, "line"), 0.95, 0, 50) < calm, "neighbours")
  test.expect(weave.misfire_chance(fire, 0.95, 0.5, 50) > calm + 0.2, "overcharge")
  test.expect(weave.misfire_chance(fire, 0.95, 0, 0) > calm, "never cast before")
  test.expect(weave.misfire_chance(fire, 0, 5, 0) <= 0.9)
  test.expect(weave.misfire_chance(fire, 1, 0, 100) >= 0.02)
end)

test.case("resolving: deterministic plans and every kind of misfire", function()
  local a = weave.resolve(w("move", { "fire" }, "line", 10), { control = 0.9, casts = 5, seed = 42 })
  local b = weave.resolve(w("move", { "fire" }, "line", 10), { control = 0.9, casts = 5, seed = 42 })
  test.expect_eq(a.misfire, b.misfire)
  test.expect_eq(a.power, b.power)
  test.expect(a.extent.reach == 60 and a.extent.radius > 0)
  local seen = {}
  for seed = 1, 400 do
    local volatile = w("create", { "fire", "water" }, "circle", 10, { "delayed" })
    local p = weave.resolve(volatile, { control = 0.2, seed = seed })
    if p.misfire then
      seen[p.misfire] = true
      if p.misfire == "weak" then test.expect_near(p.power, 4, 1e-9) end
      if p.misfire == "overshoot" then test.expect_near(p.power, 16, 1e-9) end
      if p.misfire == "late" then test.expect(p.delay >= 4) end
      if p.misfire == "stray" then test.expect(math.abs(p.offset[1]) + math.abs(p.offset[2]) > 2) end
    else
      test.expect_eq(p.delay, 3)
    end
  end
  for _, kind in ipairs({ "backfire", "stray", "weak", "late", "overshoot" }) do
    test.expect(seen[kind], "a " .. kind .. " happened")
  end
  local none, err = weave.resolve(w("move", { "fire" }, "hexagon"), {})
  test.expect(none == nil and err:find("no such form"))
  local r = weave.rng(7)
  local x = r()
  test.expect(x >= 0 and x < 1)
  local big = weave.extent(w("create", { "fire" }, "circle", 1000, { "wide", "wide", "wide", "lasting", "many" }))
  test.expect_eq(big.radius, 64)
  test.expect(big.duration == 4 and big.count == 3)
end)
