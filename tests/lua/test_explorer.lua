-- SPDX-License-Identifier: MIT
-- Unit tests of the world explorer (assets/base/scripts/ui/explorer.lua) over a small history.
local explorer = require("ui.explorer")
local history = require("people.history")
local layout = require("ui.layout")

-- A small world's history: a culture, a realm with its capital and a town, a figure, an event.
local function small_db()
  local db = history.new(1100, "settled")
  local here = world.position(0, 10, 10)
  local there = world.move(here, 90, 50000, 0, 0)
  local cu = history.add(db, "culture", { name = "Ashani", family = "steppe" })
  local capital = history.add(db, "settlement", { name = "Zarin", type = "city", population = 5000, pos = here,
    culture = cu.id })
  local town = history.add(db, "settlement", { name = "Bel", type = "town", population = 300, pos = there,
    culture = cu.id })
  local realm = history.add(db, "polity", { name = "Zarinate", capital = capital.id, culture = cu.id,
    laws = { "no magic at night" } })
  capital.realm, town.realm = realm.id, realm.id
  local ruler = history.add(db, "figure", { name = "Omid", role = "khan", polity = realm.id, born = 1040 })
  realm.ruler = ruler.id
  history.event(db, { year = 1060, kind = "founding", title = "Zarin is founded", participants = { realm.id },
    place = capital.id, importance = 0.9 })
  history.event(db, { year = 1010, kind = "settling", title = "the Ashani come", participants = { cu.id },
    importance = 0.9 })
  return db, { cu = cu, capital = capital, town = town, realm = realm, ruler = ruler, here = here }
end

local function texts(rows)
  local out = {}
  for _, r in ipairs(rows) do out[#out + 1] = r.text end
  return table.concat(out, "\n")
end

local function find(rows, pattern)
  for i, r in ipairs(rows) do
    if r.text:find(pattern, 1, true) then return i, r end
  end
end

-- spec: world-explorer: Following the lore
test.case("the lore explorer lists, links and follows", function()
  local db, e = small_db()
  local x = explorer.new()
  explorer.set_source(x, db, nil, nil)
  -- The index: every kind with its count; the first row that leads somewhere is chosen.
  local rows = explorer.rows_of(x, explorer.top(x))
  test.expect(find(rows, "Realms (1)") and find(rows, "Settlements (2)") and find(rows, "Events (2)"),
    texts(rows))
  test.expect(rows[explorer.top(x).at].go ~= nil, "on a row that leads somewhere")
  -- A list, sorted by name; events by year.
  local list = explorer.rows(x, { view = "list", of = "settlement" })
  test.expect(list[1].text:find("Bel", 1, true) and list[2].text:find("Zarin", 1, true), texts(list))
  local evs = explorer.rows(x, { view = "list", of = "event" })
  test.expect(evs[1].text:find("1010", 1, true) and evs[2].text:find("1060", 1, true), texts(evs))
  -- A realm's page: its fields with ids as links, what names it, its events.
  explorer.push(x, { view = "list", of = "polity" })
  explorer.choose(x)
  local top = explorer.top(x)
  test.expect_eq(top.id, e.realm.id)
  rows = explorer.rows_of(x, top)
  local _, cap = find(rows, "capital: Zarin")
  test.expect(cap and cap.go and cap.go.id == e.capital.id, "the capital is a link: " .. texts(rows))
  test.expect(find(rows, "laws: no magic at night"), "a list of words")
  test.expect(find(rows, "Settlements naming it (2)") and find(rows, "Figures naming it (1)"), texts(rows))
  test.expect(find(rows, "Zarin is founded"), "its events")
  -- The globe swings to the realm's capital; following the link opens it; back returns.
  test.expect(x.goal_lon ~= nil and x.mark ~= nil, "the globe turns to it")
  local want = explorer.dir_of(e.here)
  test.expect(math.abs(x.mark[1] - want[1]) < 1e-9 and math.abs(x.mark[3] - want[3]) < 1e-9, "at the capital")
  top.at = (find(rows, "capital: Zarin"))
  explorer.choose(x)
  test.expect_eq(explorer.top(x).id, e.capital.id)
  test.expect(explorer.back(x) and explorer.top(x).id == e.realm.id, "back to the realm")
  -- Moving skips the rows that lead nowhere and stops at the ends.
  top.at = 1
  explorer.move(x, 1)
  test.expect(rows[top.at].go ~= nil, "on a link")
  explorer.move(x, -100)
  test.expect(top.at >= 1)
  -- Without a history yet: one line says so.
  local y = explorer.new()
  test.expect(explorer.rows_of(y, explorer.top(y))[1].text:find("peopled", 1, true))
end)

-- spec: world-explorer: Turning, zooming and marks
test.case("the globe turns, zooms and marks", function()
  local db, e = small_db()
  local x = explorer.new()
  explorer.set_source(x, db, nil, e.here)
  local w, h = 640, 360
  -- The point looked at shows in the middle of the globe's part of the screen, the far side not at all.
  explorer.look_at(x, explorer.dir_of(e.here))
  x.lon, x.lat = x.goal_lon, x.goal_lat
  local v = explorer.view(x, w, h)
  local px, py = explorer.project(v, x.mark)
  test.expect_near(px, w * (1 - explorer.PANEL) / 2, 0.5, "in the middle of the left part")
  test.expect_near(py, h / 2, 0.5)
  test.expect_eq(explorer.project(v, { -x.mark[1], -x.mark[2], -x.mark[3] }), nil, "the far side is hidden")
  -- Marks: both settlements, the capital as such; a click on one finds it.
  local m = explorer.marks(x)
  test.expect_eq(#m, 2)
  local hit = explorer.hit(x, v, px + 1, py - 1)
  test.expect(hit and hit.id == e.capital.id and hit.capital, "the capital under the pointer")
  -- Zooming stays within its limits; the overlays turn round.
  for _ = 1, 50 do explorer.zoom(x, 0.5) end
  test.expect_eq(x.dist, explorer.MIN_DIST)
  for _ = 1, 50 do explorer.zoom(x, 2) end
  test.expect_eq(x.dist, explorer.MAX_DIST)
  test.expect(explorer.hue("polity:1") ~= nil and explorer.hue("polity:1") == explorer.hue("polity:1"))
end)

-- spec: world-explorer: Turning, zooming and marks
test.case("the explorer's keys and pointer", function()
  local db = small_db()
  local x = explorer.new()
  explorer.set_source(x, db, nil, nil)
  explorer.open(x)
  local held = {}
  local function st(name) return held[name] or {} end
  local gw, gh = layout.grid()
  -- Keys: the spin turns the globe, climb zooms in, Q/E turn the overlay, down and Enter open.
  local lon, dist = x.lon, x.dist
  held = { steer = { x = 1, y = 0 }, climb = { x = 1 } }
  explorer.tick(x, 0.5, st, nil)
  test.expect(x.lon > lon and x.dist < dist, "spun and zoomed")
  held = { tab_next = { pressed = true } }
  explorer.tick(x, 1 / 60, st, nil)
  test.expect_eq(explorer.LAYERS[x.layer], "realms")
  held = { confirm = { pressed = true } }
  explorer.tick(x, 1 / 60, st, nil)
  test.expect_eq(explorer.top(x).view, "list", "Enter opens the kind")
  held = { quit = { pressed = true } }
  explorer.tick(x, 1 / 60, st, nil)
  test.expect_eq(#x.stack, 1, "Escape goes back")
  -- The pointer: a drag turns the globe, the wheel zooms it, the panel's wheel scrolls.
  held = {}
  lon = x.lon
  explorer.tick(x, 1 / 60, st, { x = 100, y = 100, pressed = true, held = true, wheel = 0 })
  explorer.tick(x, 1 / 60, st, { x = 140, y = 100, held = true, wheel = 0 })
  explorer.tick(x, 1 / 60, st, { x = 140, y = 100, released = true, held = false, wheel = 0 })
  test.expect(x.lon < lon, "dragged")
  dist = x.dist
  explorer.tick(x, 1 / 60, st, { x = 100, y = 100, wheel = 1 })
  test.expect(x.dist < dist, "the wheel zooms in")
  local b = explorer.boxes(gw, gh)
  explorer.tick(x, 1 / 60, st, { x = b.zoom_out.x + 2, y = b.zoom_out.y + 2, pressed = true, held = true, wheel = 0 })
  test.expect(x.dist > dist, "the - chip zooms out")
  -- A row clicked opens; F4 closes.
  local rows = explorer.rows_of(x, explorer.top(x))
  local i = (find(rows, "Realms (1)"))
  explorer.tick(x, 1 / 60, st, { x = b.list.x + 4, y = b.list.y + (i - 1) * explorer.ROW + 3, pressed = true,
    held = true, wheel = 0 })
  test.expect_eq(explorer.top(x).of, "polity", "the clicked row opens")
  held = { explorer = { pressed = true } }
  explorer.tick(x, 1 / 60, st, nil)
  test.expect(not x.open, "F4 closes")
end)
