-- SPDX-License-Identifier: MIT
-- The map: the land the heir has seen is shown (by height: sea, lowland,
-- highland), unseen land stays blank; places come near are named; the heir,
-- the carpet and the goal are marked; the map moves with the stick and
-- zooms out with confirm; it is kept with the save.
-- spec: world-map: Seen and unseen
scenario { name = "world map" }
local game = demo.game
local worldmap = require("ui.worldmap")
local P = game.world_places.places
for _ = 1, 900 do
  step(1)
  if P.ready then break end
  sleep(5)
end
local m = game.worldmap
local home = game.home.pos
-- Flying 20 km east, a kilometre a second.
for k = 1, 20 do
  game.carpet.pos = world.move(home, math.pi / 2, k * 1000, 0, 0)
  game.carpet.pos.alt = math.max(world.height(game.carpet.pos), 0) + 60
  game.carpet.speed = 0
  step(60)
end
local x, y = worldmap.offset(home, game.carpet.pos)
expect(math.abs(x - 20000) < 50 and math.abs(y) < 50, string.format("20 km east: %.0f, %.0f", x, y))
local back = worldmap.at(home, x, y)
expect(world.distance(back, game.carpet.pos) < 5, "offsets go both ways")
expect(m.seen["10,0"] and m.seen["20,0"] and m.seen["10,2"], "the strip flown is seen")
expect(not m.seen["10,10"], "land 10 km north is not")
-- The view at 1 km a letter round the middle of the strip.
worldmap.centre(m, 10000, 0)
m.zoom = 1
local rows, marks = worldmap.view(m, home, game.carpet.pos, nil, nil)
local mid = rows[math.floor(#rows / 2) + 1]
local seen_letters = 0
for _, cell in ipairs(mid) do if cell[1] ~= " " then seen_letters = seen_letters + 1 end end
expect(seen_letters >= 20, "the flown strip is drawn: " .. seen_letters)
for _, cell in ipairs(rows[1]) do expect_eq(cell[1], " ", "unseen land is blank") end
local me
for _, mk in ipairs(marks) do if mk.ch == "@" then me = mk end end
expect(me and me.col == (worldmap.COLS - 1) / 2 + 10, "the heir 10 km east of the middle")
-- A place come near is named; the goal is marked.
expect(worldmap.know(m, home, "Testabad", world.move(home, math.pi / 2, 12000, 0, 0)))
expect(not worldmap.know(m, home, "Testabad", home), "named once")
local goal = world.move(home, math.pi / 2, 8000, 0, 0)
local found = {}
marks = select(2, worldmap.view(m, home, game.carpet.pos, home, goal))
for _, mk in ipairs(marks) do found[mk.ch] = mk end
expect(found.o and found.o.label == "Testabad" and found.X and found.C, "a place, the goal and the carpet")
-- From the menu: the stick moves the map, confirm zooms out.
input.inject("quit", 1, 1)
step(2)
game.menu.at = 2
input.inject("confirm", 1, 1)
step(2)
expect_eq(game.menu.page, "map")
expect(math.abs(m.cx - 20000) < 50, "the map opens on the heir")
local cx, zoom = m.cx, m.zoom
input.inject("steer", 1, 0, 3)
step(3)
expect(m.cx > cx, "moved east")
input.inject("confirm", 1, 1)
step(2)
expect(m.zoom == zoom % #worldmap.ZOOMS + 1, "zoomed out")
worldmap.draw(m, demo.assets.font, home, game.carpet.pos, nil, goal, 4, 4)
input.inject("quit", 1, 1)
step(2)
expect_eq(game.menu.page, "main")
input.inject("quit", 1, 1)
step(2)
-- Kept with the save.
local again = worldmap.new(worldmap.snapshot(m))
expect(again.seen["20,0"] and again.places[1].name == "Testabad")
expect(next(worldmap.new({ seen = 5, places = { { name = 3 } } }).seen) == nil, "a broken save keeps nothing broken")
