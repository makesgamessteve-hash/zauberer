-- SPDX-License-Identifier: MIT
-- Planet coordinates from Lua (the world table).
-- spec: planet-coordinates: Query from Lua

test.case("planet description", function()
  local p = world.planet()
  test.expect_eq(p.radius, 2000000)
  test.expect_eq(p.chunks_per_edge, 12272)
  test.expect_eq(p.chunk_size, 256)
end)

test.case("distance between positions", function()
  local r = world.planet().radius
  local a = world.from_cartesian(r, 0, 0)
  local rad = math.pi / 180
  local b = world.from_cartesian(r * math.cos(rad), r * math.sin(rad), 0)
  test.expect_near(world.distance(a, b), 34906.585, 0.1)
end)

test.case("round trip", function()
  local pos = world.position(2, 100, 200, 10.5, 20.25, 30)
  local back = world.from_cartesian(world.to_cartesian(pos))
  test.expect_eq(back.face, 2)
  test.expect_eq(back.x, 100)
  test.expect_eq(back.y, 200)
  test.expect_near(back.lx, 10.5, 0.001)
  test.expect_near(back.ly, 20.25, 0.001)
  test.expect_near(back.alt, 30, 0.001)
  local origin = world.position(0, 0, 0)
  test.expect_eq(origin.lx, 0)
  test.expect_eq(origin.alt, 0)
end)

test.case("keys and neighbours", function()
  local n = world.planet().chunks_per_edge
  local corner = world.position(4, 0, 0)
  test.expect_eq(#world.neighbors(corner), 7)
  test.expect_eq(#world.neighbors(world.position(4, 5, 5)), 8)
  test.expect_eq(world.chunk_key(world.position(5, 3, 7)), (5 << 60) | (3 << 30) | 7)
  local seen = {}
  for _, nb in ipairs(world.neighbors(world.position(0, n - 1, 10))) do
    local key = world.chunk_key(nb)
    test.expect(not seen[key], "neighbours are distinct")
    seen[key] = true
    test.expect_eq(nb.lx, 128)
  end
end)

test.case("invalid positions", function()
  test.expect_error(function() world.position(6, 0, 0) end, "not on the planet")
  test.expect_error(function() world.position(0, -1, 0) end, "not on the planet")
  test.expect_error(function() world.position(0, 0, 12272) end, "not on the planet")
  test.expect_error(function() world.to_cartesian({ face = 0, x = 1.5, y = 0 }) end, "'x' must be an integer")
  test.expect_error(function() world.distance({}, {}) end, "'face' must be an integer")
  test.expect_error(function() world.chunk_key("here") end)
end)
