-- SPDX-License-Identifier: MIT
-- Unit tests of the pocket home (assets/base/scripts/player/home.lua) and
-- the vault's shelves (player/bag.lua).
local home = require("player.home")
local bag = require("player.bag")
local pack = require("player.pack")
local news = require("player.news")

-- spec: carpet-tent: Rooms of the pocket home
test.case("rooms open and closed, walking between them, what each offers, warm inside", function()
  local h = home.new()
  test.expect(h.room == "hearth" and h.open.bedchamber and h.open.vault and not h.open.laboratory)
  test.expect(home.offers(h, "cook") and not home.offers(h, "sleep"))
  test.expect_eq(home.walk(h, 1).id, "bedchamber")
  test.expect(home.offers(h, "sleep"))
  test.expect_eq(home.walk(h, 1).id, "study")
  test.expect_eq(home.walk(h, 1).id, "vault")
  test.expect_eq(home.walk(h, 1).id, "hearth", "closed rooms are passed by")
  test.expect_eq(home.walk(h, -1).id, "vault")
  local body = home.body(h)
  test.expect(body.air == home.AIR and body.shelter and body.wind == 0 and not body.fire)
  h.room = "hearth"
  test.expect(home.body(h).fire, "the hearth burns")
  test.expect(home.open_room(h, "laboratory") and h.open.laboratory)
  test.expect(select(2, home.open_room(h, "laboratory")) == "already open")
  test.expect(select(2, home.open_room(h, "ballroom")) == "no such room")
  local found = false
  for _, line in ipairs(news.lines(h.news)) do found = found or line:find("A new room unfolds", 1, true) ~= nil end
  test.expect(found)
  local lines = home.hud_lines(h)
  test.expect(lines[1]:find("Inside the tent: Hearth room", 1, true) and lines[2]:find("[Hearth room]", 1, true))
  local back = home.new(home.snapshot(h))
  test.expect(back.open.laboratory and back.room == "hearth")
  local odd = home.new({ open = { "ballroom" }, room = "laboratory" })
  test.expect(odd.room == "hearth" and not odd.open.ballroom)
end)

-- spec: carpet-tent: The vault's shelves
test.case("in the vault, all is shelved and the needs taken off the shelves freely", function()
  local b, p = bag.new(), pack.new()
  pack.add(p, "cooked_meat", 4, 0)
  pack.add(p, "hide", 2, 0)
  bag.put(b, "wood", 120, 0)
  local moved = bag.shelve_all(b, p)
  test.expect(moved.cooked_meat == 4 and moved.wood == 120 and pack.weight(p) == 0 and bag.folds_used(b) == 0)
  bag.put(b, "boiled_water", 0, 0)
  b.vault.boiled_water = { count = 6, load = 0 }
  local out = bag.take_from_shelves(b, p)
  test.expect(out.cooked_meat == 4 and out.boiled_water == 6 and (out.hide or 0) == 0, "needs, not hides")
  test.expect(pack.weight(p) <= pack.CARRY + 1e-9 and bag.in_vault(b, "hide") == 2)
end)
