-- SPDX-License-Identifier: MIT
-- Rendered (window, 640 x 360): inside the tent, the hearth room of the
-- pocket home: tiled floor and dado, plaster ceiling, the rug and the fire.
-- spec: carpet-tent: Inside the tent
scenario { name = "home view" }
local game = demo.game
local here = world.move(game.carpet.pos, 0, 0, 0)
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded then break end
  sleep(5)
end
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
end
local c = game.carpet
c.speed = 0
c.pos = world.move(here, 0, 0, 0) -- (the same spot however long the world took)
c.heading = 0
c.pos.alt = world.ground_height(c.pos) + 1
step(2)
press("mount")
press("tent")
expect(game.tent, "pitched")
game.walker.pos = demo.tent.door(game.tent)
step(2)
press("mount")
expect(game.tent.inside, "inside")
game.hud = false
render.pin_time(0.5)
world.set_time(43200) -- (the same light whenever the world was ready)
step(10)
local ok, info = render.match_golden("home_hearth")
expect(ok, info)
