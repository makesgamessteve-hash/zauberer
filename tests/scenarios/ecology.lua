-- SPDX-License-Identifier: MIT
-- Ecology in play: the chunks around the carpet get their counts from their
-- habitat and spawn individuals from them; hares killed in a chunk are gone
-- from its count when the carpet leaves; a year later they have bred back;
-- coming back the same day brings the same animals; the counts stay within
-- their capacities and are kept with the game.
-- spec: ecology: Virtualisation
-- spec: ecology: Ecology in the game
scenario { name = "ecology" }
local ecology = require("life.ecology")
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if #world.plants(here, 100) > 20 and sim.water_at(here).loaded then break end
  sleep(5)
end
step(90)
local state = game.ecology
local r = ecology.at(state, here, world.time().time)
expect(r.k.deer > 0 and r.k.hare > 0, "deer and hares can live here")
expect(r.k.crane < r.k.hare / 4, "few cranes away from wetlands")
-- Individuals in the chunks around: as many as the counts, on average.
local live = ecology.live(state, here)
expect(live, "the carpet's chunk is virtualised")
local total, expected = 0, 0
for key, chunk in pairs(state.live) do
  for id, ids in pairs(chunk) do
    total = total + #ids
    expected = expected + state.chunks[key].n[id]
    for _, cid in ipairs(ids) do expect(sim.creature(cid).species == id) end
  end
end
expect(total > 0 and math.abs(total - expected) < 0.6 * expected + 8, total .. " animals for " .. expected)
-- Hares killed here are gone from the count once the carpet has left.
local hares = live.hare
for _ = 1, 20 do
  if #hares > 0 then break end
  ecology.fold_all(state)
  world.set_time(world.time().time + ecology.DAY)
  step(70)
  live = ecology.live(state, here)
  hares = live.hare
end
expect(#hares > 0, "hares here")
local key = tostring(world.chunk_key(here))
local before = state.chunks[key].n.hare
for _, id in ipairs(hares) do sim.hurt(id, "magic", 1000) end
local away = world.move(here, 0, 3000, 0)
game.carpet.pos = away
step(70)
expect(ecology.live(state, here) == nil, "folded back")
local after = state.chunks[key].n.hare
expect(math.abs(after - math.max(before - #hares, 0)) < 1e-6, before .. " - " .. #hares .. " = " .. after)
expect(sim.creature(hares[1]) == nil, "the dead were taken away")
-- A year later they have bred back.
world.set_time(world.time().time + ecology.YEAR * ecology.DAY)
game.carpet.pos = here
step(70)
local grown = state.chunks[key].n.hare
expect(grown > after + 0.2 * state.chunks[key].k.hare or grown > 0.9 * state.chunks[key].k.hare,
  "bred back: " .. after .. " -> " .. grown)
-- Within capacity everywhere; counts are kept with the game.
local seen = 0
for _, rec in pairs(state.chunks) do
  seen = seen + 1
  for id, n in pairs(rec.n) do expect(n >= 0 and n <= rec.k[id] + 1e-9, id .. " within capacity") end
end
expect(seen >= 18, "the chunks around both places: " .. seen)
local snap = ecology.snapshot(state)
local back = ecology.new(snap)
expect_eq(back.chunks[key].n.hare, state.chunks[key].n.hare)
-- The same day, the same animals in the same places.
local function places()
  local out = {}
  for k, chunk in pairs(state.live) do
    for id, ids in pairs(chunk) do
      for i, cid in ipairs(ids) do out[k .. id .. i] = sim.creature(cid).pos end
    end
  end
  return out
end
local function respawn() -- and look at once, before they move off
  ecology.fold_all(state)
  for _ = 1, 120 do
    step(1)
    if next(state.live) then return end
  end
end
respawn()
local first = places()
respawn()
local second, n = places(), 0
for name, pos in pairs(first) do
  n = n + 1
  expect(second[name] and world.distance(second[name], pos) < 3, "the same animal in the same place: " .. name)
end
expect(n > 0, "animals to compare")
-- Paused, the ecology folds its animals back and spawns none.
ecology.pause(state, true)
step(61)
expect(next(state.live) == nil, "paused")
ecology.pause(state, false)
step(61)
expect(ecology.live(state, here) ~= nil, "resumed")
