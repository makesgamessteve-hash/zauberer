-- SPDX-License-Identifier: MIT
-- Unit tests of the carpet as a cloak (assets/base/scripts/flight/cloak.lua)
-- and its warmth in the body (player/survival.lua).
local cloak = require("flight.cloak")
local survival = require("player.survival")

-- spec: carpet-cloak: Seen or not
test.case("drab hides, vivid awes, stillness helps, the carpet in the air shows", function()
  local k = cloak.new()
  test.expect(not k.worn and k.colour == "plain")
  local v, p = cloak.seen(k, { speed = 1 })
  test.expect(v == 1 and p == 1, "plain clothes")
  k.worn = true
  test.expect_eq(cloak.next_colour(k), "vivid")
  test.expect_eq(cloak.next_colour(k), "drab")
  local drab_still = cloak.seen(k, { speed = 0 })
  local drab_run = cloak.seen(k, { speed = 5 })
  test.expect(drab_still < 0.2 and drab_run > drab_still and drab_run < 0.5)
  k.colour = "vivid"
  v, p = cloak.seen(k, { speed = 1 })
  test.expect(v > 1 and p > 2, "vivid shows and awes")
  test.expect(cloak.seen(k, { flying = true }) == cloak.FLYING)
  test.expect(cloak.seen(k, { flying = true, silent = 0.5 }) < cloak.FLYING, "silent glide")
  k.colour = "chartreuse"
  test.expect_eq(cloak.next_colour(k), "plain")
  local back = cloak.new(cloak.snapshot(k))
  test.expect(back.worn and back.colour == "plain" and cloak.new({ colour = "pink" }).colour == "plain")
  test.expect(cloak.hud_lines(back)[1]:find("Cloaked (plain)", 1, true) and #cloak.hud_lines(cloak.new()) == 0)
end)

-- spec: carpet-cloak: Warmth in the cloak
test.case("a blizzard: the cloak keeps the body warm, dry and out of the wind", function()
  local blizzard = { air = -15, wind = 10, rain = 3, effort = 1 }
  local bare, wrapped = survival.new(), survival.new()
  local k = { worn = true }
  local hurt_bare, hurt_wrapped = 0, 0
  for _ = 1, 8 do
    for _, h in ipairs(survival.tick(bare, 3600, blizzard)) do hurt_bare = hurt_bare + h[2] end
    local ctx = { air = -15, wind = 10, rain = 3, effort = 1, cloak = cloak.warmth(k, 0) }
    for _, h in ipairs(survival.tick(wrapped, 3600, ctx)) do hurt_wrapped = hurt_wrapped + h[2] end
  end
  test.expect(bare.core < survival.CHILL and hurt_bare > 20, "without it, frozen: " .. bare.core)
  test.expect(wrapped.core >= survival.CHILL and wrapped.wet == 0, "in it, warm and dry: " .. wrapped.core)
  test.expect(hurt_wrapped < hurt_bare)
  test.expect(cloak.warmth(k, 1) < cloak.warmth(k, 0) and cloak.warmth({ worn = false }, 0) == 0, "wet, it warms less")
  test.expect(survival.feels({ air = 26, cloak = 1 }) == 26, "in the heat it adds nothing")
end)
