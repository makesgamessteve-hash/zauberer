-- SPDX-License-Identifier: MIT
-- Unit tests of the Codex (assets/base/scripts/life/codex.lua).
local codex = require("life.codex")
local news = require("player.news")

local function said(c, text)
  for _, line in ipairs(news.lines(c.news)) do
    if line:find(text, 1, true) then return true end
  end
  return false
end

local function has(lines, text)
  for _, line in ipairs(lines) do
    if line:find(text, 1, true) then return true end
  end
  return false
end

-- spec: codex: Entries and levels
test.case("seen, observed by looking, studied by sampling, understood", function()
  local all = codex.entries()
  test.expect(all["plant:oak"] and all["fungus:fly_agaric"] and all["animal:wolf"] and all["insect:mosquito"])
  local c = codex.new()
  -- In sight but off the middle: seen, quietly; not looked at.
  codex.see(c, { { id = "animal:deer", dist = 80, angle = 0.4, allow = 0.01 } })
  test.expect(codex.level(c, "animal:deer") == codex.SEEN and c.focus == nil and #news.lines(c.news) == 0)
  -- Too far or behind: not seen.
  codex.see(c, { { id = "plant:oak", dist = 400, angle = 0, allow = 0 },
    { id = "plant:beech", dist = 10, angle = 2, allow = 0 }, { id = "plant:nothing", dist = 1, angle = 0 } })
  test.expect(codex.level(c, "plant:oak") == 0 and codex.level(c, "plant:beech") == 0)
  -- Looked at: the one nearest the middle (a big thing counts its size).
  codex.see(c, { { id = "animal:deer", dist = 60, angle = 0.1, allow = 0.02 },
    { id = "plant:oak", dist = 20, angle = 0.3, allow = 0.25 } })
  test.expect(c.focus == "plant:oak", tostring(c.focus))
  codex.watch(c, 2)
  test.expect(codex.level(c, "plant:oak") == codex.SEEN)
  codex.watch(c, 1.5)
  test.expect(codex.level(c, "plant:oak") == codex.OBSERVED and said(c, "Codex: Oak observed"))
  -- Animals take longer.
  codex.see(c, { { id = "animal:wolf", dist = 50, angle = 0.05, allow = 0.01 } })
  codex.watch(c, 4)
  test.expect(codex.level(c, "animal:wolf") == codex.SEEN)
  test.expect(has(codex.hud_lines(c), "Looking at grey wolf 50%"), table.concat(codex.hud_lines(c), " | "))
  codex.watch(c, 4)
  test.expect(codex.level(c, "animal:wolf") == codex.OBSERVED and said(c, "Grey wolf observed - it may attack!"))
  -- Samples study, and three understand; the right method gives more.
  test.expect(codex.method(c, "animal:hare") == 1)
  codex.sampled(c, { "animal:hare" })
  test.expect(codex.level(c, "animal:hare") == codex.STUDIED and said(c, "Brown hare studied"))
  codex.sampled(c, { "animal:hare", "plant:nothing" })
  codex.sampled(c, { "animal:hare" })
  test.expect(codex.level(c, "animal:hare") == codex.UNDERSTOOD and codex.method(c, "animal:hare") == codex.METHOD)
  -- Insects cannot be sampled yet: watching long studies and understands them.
  codex.see(c, { { id = "insect:bee", dist = 5, angle = 0, allow = 0.3 } })
  codex.watch(c, 31)
  test.expect(codex.level(c, "insect:bee") == codex.STUDIED)
  codex.watch(c, 60)
  test.expect(codex.level(c, "insect:bee") == codex.UNDERSTOOD)
  local n = codex.counts(c)
  test.expect(n[1] == 5 and n[2] == 4 and n[3] == 2 and n[4] == 2, table.concat(n, " "))
  -- News fades in real time.
  news.tick(c.news, news.TIME + 1)
  test.expect(#news.lines(c.news) == 0)
end)

-- spec: codex: The Codex warns
test.case("warnings from observed, and what the pages tell by level", function()
  local c = codex.new()
  test.expect(codex.danger("fungus:fly_agaric") == "poisonous" and codex.danger("fungus:chanterelle") == nil)
  test.expect(codex.danger("insect:mosquito") and codex.danger("animal:deer") == nil and codex.danger("x") == nil)
  test.expect(codex.warned(c, "boiled_water")) -- (an item from nothing is plain to see)
  test.expect(not codex.warned(c, "fly_agaric"))
  codex.see(c, { { id = "fungus:fly_agaric", dist = 2, angle = 0, allow = 0.4 } })
  test.expect(not codex.warned(c, "fly_agaric"))
  test.expect(has(codex.page(c, "fungus:fly_agaric"), "Look at it longer"))
  test.expect(not has(codex.page(c, "fungus:fly_agaric"), "Beware"))
  codex.watch(c, 3)
  test.expect(codex.warned(c, "fly_agaric") and said(c, "Fly agaric observed - poisonous!"))
  local page = codex.page(c, "fungus:fly_agaric")
  test.expect(page[1] == "Fly agaric: a fungus (observed)" and has(page, "Beware: poisonous"))
  test.expect(has(page, "Fruits in forests, summer to autumn") and has(page, "Take a sample"),
    table.concat(page, " | "))
  -- A plant's page grows with the level.
  test.expect(#codex.page(c, "plant:meadow_flowers") == 0)
  codex.sampled(c, { "plant:meadow_flowers" })
  page = codex.page(c, "plant:meadow_flowers")
  test.expect(has(page, "Gives healing herbs (in flower)"), table.concat(page, " | "))
  test.expect(has(page, "In flowers spring to summer; fruit summer to autumn"), table.concat(page, " | "))
  test.expect(not has(page, "eases sickness"))
  codex.sampled(c, { "plant:meadow_flowers" })
  codex.sampled(c, { "plant:meadow_flowers" })
  page = codex.page(c, "plant:meadow_flowers")
  test.expect(has(page, "Healing herbs: eases sickness") and has(page, "steeped in boiled water: herbal tea"))
  test.expect(has(page, "a quarter more") and not has(page, "learn more"), table.concat(page, " | "))
  -- An animal's page.
  codex.sampled(c, { "animal:wolf", "animal:wolf", "animal:wolf" })
  page = codex.page(c, "animal:wolf")
  test.expect(has(page, "Hunts brown hare, red deer, wild boar") and has(page, "Butchered, gives hide 1, raw meat 6"))
  test.expect(has(page, "Afraid of fire") and has(page, "smells you 300 m downwind"), table.concat(page, " | "))
  -- An insect's page from its data.
  codex.see(c, { { id = "insect:mosquito", dist = 5, angle = 0, allow = 0.3 } })
  codex.watch(c, 3)
  page = codex.page(c, "insect:mosquito")
  test.expect(has(page, "Dances over wet ground") and has(page, "Beware: it bites") and has(page, "Watch it longer"))
  -- A tree's page.
  codex.sampled(c, { "plant:oak" })
  page = codex.page(c, "plant:oak")
  test.expect(page[1]:find("a tree", 1, true) and has(page, "Grows where the year is 3 to 19 C"))
  test.expect(has(page, "In flowers spring; fruit autumn") and has(page, "acorns (in fruit)"),
    table.concat(page, " | "))
end)

-- spec: codex: Reading the Codex and keeping it
test.case("browsing, and the Codex kept with the game", function()
  local c = codex.new()
  test.expect(codex.browse(c) == nil and said(c, "The Codex is empty"))
  codex.sampled(c, { "plant:oak" })
  codex.sampled(c, { "fungus:porcini" })
  test.expect(codex.browse(c) == "fungus:porcini") -- the last learned first
  test.expect(has(codex.hud_lines(c), "Codex 1/2"))
  test.expect(has(codex.hud_lines(c), "  Porcini: a fungus (studied)"))
  test.expect(codex.browse(c) == "plant:oak" and codex.browse(c) == nil and c.open == nil)
  local back = codex.new(codex.snapshot(c))
  test.expect(codex.level(back, "plant:oak") == codex.STUDIED and codex.level(back, "fungus:porcini") == codex.STUDIED)
  test.expect(back.known["plant:oak"].samples == 1 and back.order == c.order)
  -- Broken or old saves keep what they can.
  local odd = codex.new({ known = { ["plant:oak"] = { level = 9 }, ["plant:gone"] = { level = 2 },
    ["plant:fir"] = 3 } })
  test.expect(codex.level(odd, "plant:oak") == codex.UNDERSTOOD and odd.known["plant:gone"] == nil)
  test.expect(odd.known["plant:fir"] == nil and next(codex.new("junk").known) == nil)
end)
