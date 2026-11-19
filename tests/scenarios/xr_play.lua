-- SPDX-License-Identifier: MIT
-- VR play with an emulated headset (xr.emulate, xr.emulate_controller): the
-- carpet flies on the thumbstick and the reins, snap turns, a Fireball is cast
-- by drawing a line in the air with the left hand, and the HUD is a panel low
-- ahead (checked in the pixels when rendered: tests/engine/test_render.py).
-- spec: webxr-play: Controllers reach Lua
-- spec: webxr-play: Flying in VR
-- spec: webxr-play: Casting by drawing in the air
-- spec: webxr-play: HUD readable in the headset
scenario { name = "vr play" }
local game = demo.game
local c = game.carpet
expect_eq(game.mode, "flight")
for _ = 1, 900 do -- (the land around the carpet, for the fire a Fireball makes)
  step(1)
  if sim.fire_at(game.carpet.pos).loaded then break end
  sleep(5)
end
local rendered = render.stats().backend == "gl"

local function wait_until(done)
  for _ = 1, 1300 do
    if done() then return end
    step(1)
  end
end

local function q(x, y, z, degrees) -- a rotation about an axis
  local h = math.rad(degrees) / 2
  return { x * math.sin(h), y * math.sin(h), z * math.sin(h), math.cos(h) }
end
local LEVEL = { 0, 0, 0, 1 }
local RIGHT_HAND = { position = { 0.2, -0.3, -0.35 }, orientation = LEVEL }
local LEFT_HAND = { position = { -0.2, -0.3, -0.35 }, orientation = LEVEL }

-- The headset at the seat, both controllers held level.
xr.emulate({ position = { 0, 0, 0 } })
xr.emulate_controller("right", RIGHT_HAND)
xr.emulate_controller("left", LEFT_HAND)
step(1)
wait_until(function() return xr.state().frames >= 2 end)
local s = xr.state()
expect(s.active and s.emulated, "the emulated session runs")
local r = s.controllers.right
expect(r.tracked and s.controllers.left.tracked, "both controllers are tracked")
for i = 1, 3 do expect_near(r.pose.position[i], RIGHT_HAND.position[i], 1e-5, "controller position") end
expect_near(r.pose.orientation[4], 1, 1e-6)
expect_eq(r.trigger, 0)
step(2)
expect(game.xrp and game.xrp.neutral and game.xrp.neutral.right, "the neutral pose is taken")

-- The left thumbstick pushed forward: the carpet speeds up.
xr.emulate_controller("left", LEFT_HAND, { 0, 1 })
expect_near(xr.state().controllers.left.axes[2], 1, 1e-6, "the thumbstick reaches Lua at once")
step(120)
expect(c.speed > 30, "the thumbstick is the throttle: " .. c.speed)
xr.emulate_controller("left", LEFT_HAND)

-- The reins rolled 30 degrees to the right: a right turn, the vignette darkens.
local heading = c.heading
xr.emulate_controller("right", { position = RIGHT_HAND.position, orientation = q(0, 0, -1, 30) })
step(60)
expect(demo.carpet.wrap(c.heading - heading) > 0.3, "reins turn the carpet")
expect(game.xrp.vignette > 0.2, "the comfort vignette while turning: " .. game.xrp.vignette)
-- Tilted nose up: a climb.
local alt = c.pos.alt
xr.emulate_controller("right", { position = RIGHT_HAND.position, orientation = q(1, 0, 0, 25) })
step(90)
expect(c.pos.alt > alt + 15, "the reins pitched up climb")
xr.emulate_controller("right", RIGHT_HAND)
step(30)

-- Snap turning: the same reins turn the carpet in 30 degree steps.
game.xrp = nil
game.xr_options = { snap_turn = true }
step(2)
heading = c.heading
xr.emulate_controller("right", { position = RIGHT_HAND.position, orientation = q(0, 0, -1, 45) })
step(1)
expect_near(demo.carpet.wrap(c.heading - heading), math.rad(30), 0.02, "one snap at once")
xr.emulate_controller("right", RIGHT_HAND)
step(30)
game.xr_options = nil
game.xrp = nil
step(2)

-- Casting: the left trigger held charges the left hand's Fireball while the
-- controller draws a line across the plane facing the player; letting go casts.
local magic = demo.magic
local drawn = {}
local give = magic.gesture
magic.gesture = function(p, g, hand)
  drawn[#drawn + 1] = { form = g.form, hand = hand, charging = p.charging and p.charging.hand }
  return give(p, g, hand)
end
local before = game.last_cast
for i = 0, 40 do
  local x = -0.25 + 0.5 * i / 40
  xr.emulate_controller("left", { position = { x, -0.2, -0.4 }, orientation = LEVEL }, nil, 1)
  step(1)
end
expect(game.magic.charging and game.magic.charging.hand == "left", "the trigger charges the left hand")
xr.emulate_controller("left", LEFT_HAND, nil, 0)
step(3)
magic.gesture = give
expect_eq(#drawn, 1, "one gesture drawn in the air")
expect_eq(drawn[1] and drawn[1].form, "line")
expect_eq(drawn[1] and drawn[1].hand, "left")
expect_eq(drawn[1] and drawn[1].charging, "left", "it shapes the left hand's charge")
local cast = game.last_cast
expect(cast ~= before and cast and cast.ok and cast.name == "Fireball", cast and cast.message)

-- The HUD: a head-locked panel low ahead, in both eyes.
game.hud = true
step(2)
expect(render.stats().glyphs > 0, "the HUD is drawn")
if rendered then
  render.pin_time(10)
  local function capture()
    render.capture("window")
    local px = {}
    for y = 0, 359 do
      for x = 0, 639 do
        local cr, cg, cb = render.pixel(x, y)
        px[y * 640 + x] = cr * 65536 + cg * 256 + cb
      end
    end
    return px
  end
  c.speed, c.pitch, c.bank = 0, 0, 0 -- the same scene twice: the carpet hovering
  step(60)
  local with = capture()
  game.hud = false
  step(1)
  local without = capture()
  game.hud = true
  render.pin_time(nil)
  local function changed(x0, x1, y0, y1)
    local n = 0
    for y = y0, y1 do
      for x = x0, x1 do
        if with[y * 640 + x] ~= without[y * 640 + x] then n = n + 1 end
      end
    end
    return n
  end
  -- Straight ahead lies right of the left eye's centre and left of the right eye's.
  expect(changed(130, 245, 190, 250) > 150, "the panel in the left eye")
  expect(changed(395, 510, 190, 250) > 150, "the panel in the right eye")
  expect(changed(0, 100, 0, 40) + changed(320, 420, 0, 40) < 40, "nothing in the top corners")
end

-- Leaving VR: the chase camera again.
xr.emulate(false)
step(1)
wait_until(function() return not xr.state().active end)
expect(not xr.state().active, "the session ended")
step(5)
expect_eq(render.stats().views, 1)
