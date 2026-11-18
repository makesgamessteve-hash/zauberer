-- SPDX-License-Identifier: MIT
-- Unit tests of plant life cycles and items (assets/base/scripts/life).
local plants = require("life.plants")
local items = require("life.items")

local DAY = plants.DAY

-- spec: plant-life-cycle: Life cycles and items are data
test.case("life cycles and items are checked", function()
  local all = items.all()
  test.expect(all.acorns and all.wood and all.healing_herbs, "base items")
  test.expect_eq(items.name("acorns"), "Acorns")
  test.expect_eq(items.name("nothing"), "nothing")
  local cases = {
    { {}, "no items" },
    { { { name = "x", kind = "food", weight = 1 } }, "no id" },
    { { { id = "a", kind = "food", weight = 1 } }, "no name" },
    { { { id = "a", name = "A", kind = "toy", weight = 1 } }, "unknown kind" },
    { { { id = "a", name = "A", kind = "food", weight = 0 } }, "weight" },
    { { { id = "a", name = "A", kind = "food", weight = 1 }, { id = "a", name = "A", kind = "food", weight = 1 } },
      "twice" },
  }
  for _, c in ipairs(cases) do
    local ok, err = items.check(c[1])
    test.expect(not ok and err:find(c[2], 1, true), tostring(err))
  end
  local cycles = plants.cycles()
  local flora = world.flora()
  local with = 0
  for _, sp in ipairs(flora) do
    if cycles[sp.id] then with = with + 1 end
  end
  test.expect_eq(with, #flora, "every flora species has a life cycle")
  local bad = {
    { { oak = { parts = { { item = "gold", yield = { 1, 2 }, regrow = 1 } } } }, "unknown item" },
    { { oak = { leaves = { 5, 6 }, parts = {} } }, "leaves" },
    { { oak = { parts = { { item = "acorns", when = "fruit", yield = { 1, 2 }, regrow = 1 } } } }, "when" },
    { { oak = { parts = { { item = "acorns", yield = { 0, 2 }, regrow = 1 } } } }, "yield" },
    { { oak = { parts = { { item = "acorns", yield = { 1, 2 } } } } }, "regrow" },
    { { oak = {} }, "no parts" },
  }
  for _, c in ipairs(bad) do
    local ok, err = plants.check(c[1], all)
    test.expect(not ok and err:find(c[2], 1, true), tostring(err))
  end
end)

-- spec: plant-life-cycle: Plants through the seasons
test.case("stages follow the local season, each plant its own way", function()
  local oak = { id = 1, species = "oak", variant = 128, size = 1 }
  test.expect_eq(plants.stage(oak, 0.45).name, "in flower")
  test.expect_eq(plants.stage(oak, 1.5).name, "in leaf")
  test.expect_eq(plants.stage(oak, 2.3).name, "in fruit")
  test.expect_eq(plants.stage(oak, 3.5).name, "bare")
  local hazel = { id = 2, species = "hazel", variant = 128 }
  test.expect(plants.stage(hazel, 3.9).flowers and plants.stage(hazel, 0.1).flowers, "catkins across the new year")
  local spruce = { id = 3, species = "spruce", variant = 0 }
  test.expect(plants.stage(spruce, 3.5).leaves, "evergreen")
  -- Early and late plants: a week either way near the edge of a stage.
  local early, late = { id = 4, species = "oak", variant = 0 }, { id = 5, species = "oak", variant = 255 }
  test.expect(plants.stage(early, 1.97).fruit and not plants.stage(late, 1.97).fruit, "each plant its own timing")
  test.expect_eq(plants.stage({ id = 6, species = "unknown" }, 1).name, "bare")
end)

-- spec: plant-life-cycle: Taking parts of plants
test.case("parts by season and size, again after they regrow", function()
  local state = plants.new()
  local oak = { id = 77, species = "oak", variant = 128, size = 1 }
  local now = 10 * DAY
  local function has(list, item)
    for _, a in ipairs(list) do
      if a.item == item then return a end
    end
  end
  local autumn = plants.available(state, oak, now, 2.3)
  test.expect(has(autumn, "acorns") and has(autumn, "oak_bark") and has(autumn, "wood"))
  test.expect(not has(plants.available(state, oak, now, 0.9), "acorns"), "no acorns in summer")
  local big = has(autumn, "acorns").count
  local small = has(plants.available(state, { id = 77, species = "oak", variant = 128, size = 0 }, now, 2.3), "acorns")
  test.expect(big > small.count and big >= 90 and small.count <= 30, big .. " vs " .. small.count)
  local item, n = plants.take(state, oak, "acorns", now, 2.3)
  test.expect(item == "acorns" and n == big)
  local again, why = plants.take(state, oak, "acorns", now + DAY, 2.4)
  test.expect(again == nil and why:find("no Acorns"), why)
  test.expect(has(plants.available(state, oak, now + 41 * DAY, 2.3), "acorns"), "a year later")
  -- The same harvest gives the same count, another year another.
  local counts = {}
  for year = 0, 5 do
    counts[#counts + 1] = has(plants.available(plants.new(), oak, year * plants.YEAR * DAY, 2.3), "acorns").count
  end
  local differ = false
  for k = 2, #counts do differ = differ or counts[k] ~= counts[1] end
  test.expect(differ, "yields vary by year")
  test.expect_eq(has(plants.available(plants.new(), oak, 0, 2.3), "acorns").count, counts[1])
  -- A whole part of a plant that is not loaded cannot be taken.
  local gone, msg = plants.take(state, oak, "wood", now, 2.3)
  test.expect(gone == nil and msg == "it is gone", msg)
  -- Saved: only recent records.
  local snap = plants.snapshot(state, now + 2 * DAY)
  test.expect(snap.taken["77"].acorns == now)
  local back = plants.new(snap)
  test.expect(not has(plants.available(back, oak, now + DAY, 2.3), "acorns"), "remembered after loading")
  test.expect(next(plants.snapshot(state, now + 60 * DAY).taken) == nil, "old records dropped")
  test.expect_eq(#plants.available(state, { id = 1, species = "unknown" }, now, 1), 0)
end)
