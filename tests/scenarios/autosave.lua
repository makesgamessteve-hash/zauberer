-- SPDX-License-Identifier: MIT
-- The game saves on its own: at a turn of the story, before sleep, and once
-- a defeated heir is up again (not while they lie defeated); a line on the
-- HUD says so for two seconds; a client in company does not save. The
-- scenario ends without quitting (as if the game were cut off): the next
-- start continues from the last autosave (tests/engine/test_headless.py).
-- spec: world-persistence: Progress survives a lost game
scenario { name = "autosave" }
local game = demo.game
local P = game.world_places.places
for _ = 1, 900 do
  step(1)
  if P.ready and world.chunk_stage(game.carpet.pos) >= 2 then break end
  sleep(5)
end
game.save_name = "auto"
local function hud_has(text)
  for _, line in ipairs(demo.flight_lines()) do
    if (type(line) == "table" and line.raw or line):find(text, 1, true) then return true end
  end
  return false
end
local function saved() return table.concat(world.saves(), " "):find("auto", 1, true) ~= nil end
expect(not saved(), "nothing saved yet")
-- A turn of the story: F5, then the chores done and a word with Farrokh.
input.inject("story", 1, 1)
step(3)
local S = game.story
expect(S, "the story began")
S.chores = { water = true, goats = true, pomegranates = true }
S.talked = true
step(2)
expect_eq(S.beat, "arrival")
expect(saved() and game.saved_at, "saved at the turn of the story")
expect(hud_has("Game saved"), "the HUD says so")
local at = game.saved_at
step(60 * 3)
expect(not hud_has("Game saved"), "for two seconds")
-- Defeated: not saved while down, saved once up again.
local health = demo.health
health.hurt(game.health, "magic", 1000)
step(2)
expect(not game.autosave("time"), "not while defeated")
expect(game.saved_at == at)
step(60 * 4)
expect(not game.health.down and game.saved_at > at, "saved once up again")
-- Before sleep (on foot).
game.walker = demo.walker.new(world.move(game.carpet.pos, 0, 0, 0, 0), 0,
  { move = world.move, height = world.ground_height, water = world.water })
game.walker.pos.alt = world.ground_height(game.walker.pos)
step(2)
at = game.saved_at
input.inject("sleep", 1, 1)
step(3)
expect(game.saved_at > at, "saved before sleep")
-- A client does not save.
local role = game.coop.role
game.coop.role = "client"
expect(not game.autosave("time"), "a client leaves saving to the host")
game.coop.role = role
-- The last autosave: the story at its arrival.
expect(game.autosave("story"))
