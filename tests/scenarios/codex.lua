-- SPDX-License-Identifier: MIT
-- The Codex in the game: on foot the mage looks at a plant until it is
-- observed and follows a deer with the eyes until it is too; the HUD says so.
-- Butchering studies an animal, and once understood the right method gives
-- more meat. The Codex warns of the fly agaric only once it is observed. J
-- opens the Codex; it is kept with the game.
-- spec: codex: The Codex in the game
scenario { name = "codex" }
local game = demo.game
local codex = demo.codex
local pack = demo.pack
local cooking = demo.cooking
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if #world.plants(here, 100) > 20 and sim.fire_at(here).loaded then break end
  sleep(5)
end
demo.ecology.pause(game.ecology, true)
local cx = game.codex
local function news(text)
  for _, line in ipairs(demo.flight_lines()) do
    if line:find(text, 1, true) then return true end
  end
  return false
end
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
end
-- Step off by a tree.
local tree
for _, pl in ipairs(world.plants(here, 150)) do
  if not tree and pl.layer == "tree" then tree = pl end
end
expect(tree, "a tree near")
local c = game.carpet
c.pos = world.move(tree.pos, 0, 12, 0)
c.pos.alt = world.ground_height(c.pos) + 1
c.speed = 0
step(2)
press("mount")
local w = game.walker
expect(w ~= nil, "on foot")
-- Look at it: the eyes on its middle.
local function look_at(pos, up)
  local a, b = world.to_local(w.pos), world.to_local(pos)
  local dx, dy, dz = b[1] - a[1], b[2] + (up or 0) - a[2] - 1.7, b[3] - a[3]
  w.heading = math.atan(dx, -dz)
  w.pitch = math.atan(dy, math.sqrt(dx * dx + dz * dz))
end
local id = "plant:" .. tree.species
local observed = false
for _ = 1, 60 * 6 do
  look_at(tree.pos, tree.height / 2)
  step(1)
  if codex.level(cx, id) >= codex.OBSERVED then
    observed = true
    break
  end
end
expect(observed, "the tree observed by looking at it")
expect(codex.level(cx, id) >= codex.OBSERVED and news(codex.entries()[id].name .. " observed"), "and the HUD says so")
-- A deer, followed with the eyes, takes longer.
local deer = sim.spawn_creature("deer", world.move(w.pos, w.heading + 2, 25, 0))
local ticks = 0
for _ = 1, 60 * 14 do
  ticks = ticks + 1
  local d = sim.creature(deer)
  if d then look_at(d.pos, 0.4) end
  step(1)
  if codex.level(cx, "animal:deer") >= codex.OBSERVED then break end
end
expect(codex.level(cx, "animal:deer") == codex.OBSERVED, "the deer observed")
expect(ticks / 60 >= codex.LOOK.animal - 0.5, "after looking longer: " .. ticks)
-- Butchering studies; once understood, the right method gives more.
local function butcher_hare()
  local hare = sim.spawn_creature("hare", world.move(w.pos, w.heading, 1, 0))
  sim.hurt(hare, "magic", 1000)
  step(2)
  local before = pack.count(game.pack, "raw_meat")
  press("gather")
  pack.take(game.pack, "hide", 9) -- (hides are heavy)
  return pack.count(game.pack, "raw_meat") - before
end
expect(butcher_hare() == 3 and codex.level(cx, "animal:hare") == codex.STUDIED and news("Brown hare studied"))
butcher_hare()
expect(butcher_hare() == 3 and codex.level(cx, "animal:hare") == codex.UNDERSTOOD, "understood after three")
expect(butcher_hare() == 4, "the right method gives a quarter more")
-- The fly agaric: eaten unknowing, avoided once observed.
local s = game.survival
local function eats_agaric()
  local p = pack.new()
  pack.add(p, "fly_agaric", 1, 0)
  pack.add(p, "chanterelles", 1, 0)
  return cooking.eat(p, s, function(item) return codex.warned(cx, item) end) == "Fly agaric"
end
expect(eats_agaric(), "an unknown mushroom is tasted")
s.poison = 0
codex.see(cx, { { id = "fungus:fly_agaric", dist = 1, angle = 0, allow = 0.5 } })
codex.watch(cx, codex.LOOK.fungus)
expect(news("Fly agaric observed - poisonous!") and not eats_agaric(), "the Codex warns")
-- J opens the Codex at the entry last learned; it is kept with the game.
press("codex")
expect(news("Codex 1/") and news("Fly agaric: a fungus (observed)"), "the Codex is open")
local back = codex.new(codex.snapshot(cx))
expect(codex.level(back, "animal:hare") == codex.UNDERSTOOD and codex.level(back, id) >= codex.OBSERVED, "kept")
demo.ecology.pause(game.ecology, false)
