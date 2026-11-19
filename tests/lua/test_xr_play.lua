-- SPDX-License-Identifier: MIT
-- Unit tests of VR play (assets/base/scripts/xr/play.lua) with fake xr.state() tables.
local xrplay = require("xr.play")
local carpet = require("flight.carpet")
local magic = require("magic.play")

local DT = 1 / 60

local function axis(x, y, z, degrees)
  local l = math.sqrt(x * x + y * y + z * z)
  local h = math.rad(degrees) / 2
  local s = math.sin(h) / l
  return { x * s, y * s, z * s, math.cos(h) }
end

local function qmul(a, b)
  return {
    a[4] * b[1] + a[1] * b[4] + a[2] * b[3] - a[3] * b[2],
    a[4] * b[2] - a[1] * b[3] + a[2] * b[4] + a[3] * b[1],
    a[4] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[4],
    a[4] * b[4] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3],
  }
end

local IDENTITY = { 0, 0, 0, 1 }

local function controller(orientation, fields)
  local c = { tracked = orientation ~= nil,
    pose = { position = { 0.2, -0.3, -0.4 }, orientation = orientation or IDENTITY },
    axes = { 0, 0 }, trigger = 0, squeeze = 0 }
  for k, v in pairs(fields or {}) do c[k] = v end
  return c
end

-- A fake xr.state(): the head's orientation and the controllers.
local function state(head, left, right)
  return { active = true, views = 2, head = { position = { 0, 0, 0 }, orientation = head or IDENTITY },
    controllers = { left = left or controller(nil), right = right or controller(nil) } }
end

local function still_carpet()
  return { heading = 0, speed = 0 }
end

-- spec: webxr-play: Steering with the reins
test.case("reins: roll turns, pitch climbs, relative to the neutral pose", function()
  local p = xrplay.new()
  test.expect_eq(p.options.steer, "reins")
  test.expect_eq(xrplay.tick(p, { active = false }, DT), nil, "nothing without a session")
  -- The reins held tilted 20 degrees forward when the session starts: that is neutral.
  local neutral = axis(1, 0, 0, -20)
  local xin = xrplay.tick(p, state(nil, nil, controller(neutral)), DT, still_carpet())
  test.expect_near(xin.turn, 0, 1e-9)
  test.expect_near(xin.climb, 0, 1e-9)
  test.expect_eq(xin.throttle, 0)
  -- Rolled 30 degrees to the right (clockwise seen from behind): a right turn.
  local rolled = qmul(neutral, axis(0, 0, -1, 30))
  xin = xrplay.tick(p, state(nil, nil, controller(rolled)), DT, still_carpet())
  test.expect(xin.turn > 0.6 and xin.turn < 0.8, "turn " .. xin.turn)
  test.expect_near(xin.climb, 0, 1e-6)
  -- Fully to the left.
  xin = xrplay.tick(p, state(nil, nil, controller(qmul(neutral, axis(0, 0, -1, -60)))), DT, still_carpet())
  test.expect_near(xin.turn, -1, 1e-9)
  -- Nose up 20 degrees: a climb; inside the dead zone: nothing.
  xin = xrplay.tick(p, state(nil, nil, controller(qmul(neutral, axis(1, 0, 0, 20)))), DT, still_carpet())
  test.expect(xin.climb > 0.5, "climb " .. xin.climb)
  test.expect_near(xin.turn, 0, 1e-6)
  xin = xrplay.tick(p, state(nil, nil, controller(qmul(neutral, axis(1, 0, 0, -3)))), DT, still_carpet())
  test.expect_eq(xin.climb, 0)
  -- The rein hand untracked: the other hand holds the reins.
  local q = p.neutral.left
  test.expect_eq(q, nil, "the left hand was never tracked")
  xin = xrplay.tick(p, state(nil, controller(IDENTITY), nil), DT, still_carpet())
  test.expect_near(xin.turn, 0, 1e-9, "its neutral is taken when it first appears")
  xin = xrplay.tick(p, state(nil, controller(axis(0, 0, -1, 45)), nil), DT, still_carpet())
  test.expect_near(xin.turn, 1, 1e-9)
  -- While the rein hand draws (trigger held) it does not steer.
  xin = xrplay.tick(p, state(nil, controller(axis(0, 0, -1, 45), { trigger = 1 }), nil), DT, still_carpet())
  test.expect_eq(xin.turn, 0)
  test.expect(xin.casts.left.pressed and xin.casts.left.held)
end)

-- spec: webxr-play: Recentring
test.case("a squeeze held for a second recentres", function()
  local p = xrplay.new()
  local tilted = axis(0, 0, -1, 30)
  xrplay.tick(p, state(nil, nil, controller(IDENTITY)), DT, still_carpet())
  test.expect(xrplay.tick(p, state(nil, nil, controller(tilted)), DT).turn > 0.5)
  local recentred = 0
  for _ = 1, 70 do -- squeezing at the tilted pose for more than a second
    local xin = xrplay.tick(p, state(nil, nil, controller(tilted, { squeeze = 1 })), DT)
    if xin.recentred then recentred = recentred + 1 end
  end
  test.expect_eq(recentred, 1, "once per squeeze")
  test.expect_near(xrplay.tick(p, state(nil, nil, controller(tilted)), DT).turn, 0, 1e-6, "the new neutral")
  -- A short squeeze does nothing; a new session takes a new neutral pose.
  for _ = 1, 30 do xrplay.tick(p, state(nil, nil, controller(IDENTITY, { squeeze = 1 })), DT) end
  test.expect_near(xrplay.tick(p, state(nil, nil, controller(IDENTITY)), DT).turn, -25 / 35, 1e-6)
  xrplay.tick(p, { active = false }, DT)
  test.expect_near(xrplay.tick(p, state(nil, nil, controller(IDENTITY)), DT).turn, 0, 1e-9)
end)

-- spec: webxr-play: Steering with the head
test.case("head steering: looking right turns, looking up climbs", function()
  local p = xrplay.new({ steer = "head" })
  xrplay.tick(p, state(), DT)
  local xin = xrplay.tick(p, state(axis(0, 1, 0, -40)), DT) -- turned 40 degrees to the right
  test.expect(xin.turn > 0.8 and xin.turn < 1, "turn " .. xin.turn)
  xin = xrplay.tick(p, state(axis(0, 1, 0, 60)), DT)
  test.expect_near(xin.turn, -1, 1e-9)
  xin = xrplay.tick(p, state(axis(1, 0, 0, 25)), DT) -- looking up
  test.expect(xin.climb > 0.5, "climb " .. xin.climb)
  test.expect_near(xin.turn, 0, 1e-6)
  xin = xrplay.tick(p, state(axis(0, 1, 0, 5)), DT) -- a glance: the dead zone
  test.expect_eq(xin.turn, 0)
end)

-- spec: webxr-play: Thumbsticks
test.case("thumbsticks: throttle, turning and climbing", function()
  local p = xrplay.new()
  local xin = xrplay.tick(p, state(nil, controller(IDENTITY, { axes = { 0.5, 1 } }),
    controller(IDENTITY, { axes = { 0, -0.75 } })), DT)
  test.expect_eq(xin.throttle, 1)
  test.expect_eq(xin.turn, 0.5)
  test.expect_eq(xin.climb, -0.75)
  xin = xrplay.tick(p, state(nil, controller(IDENTITY, { axes = { 0.1, -0.1 } })), DT)
  test.expect_eq(xin.throttle, 0, "dead zone")
  test.expect_eq(xin.turn, 0)
  -- Added to the other devices' flight input.
  local fin = xrplay.flight({ turn = 0.8, throttle = 0.5, climb = 0, boost = true },
    { turn = 0.5, throttle = 0.25, climb = -0.5, snap = 0.1, scale = 0.5 })
  test.expect_eq(fin.turn, 1)
  test.expect_eq(fin.throttle, 0.75)
  test.expect_eq(fin.climb, -0.5)
  test.expect(fin.boost)
  test.expect_eq(fin.snap, 0.1)
  test.expect_eq(fin.scale, 0.5)
end)

-- spec: webxr-play: Snap turning
test.case("snap turning in 30 degree steps with a cooldown", function()
  local p = xrplay.new({ snap_turn = true })
  xrplay.tick(p, state(nil, nil, controller(IDENTITY)), DT, still_carpet())
  local right = controller(axis(0, 0, -1, 45))
  local snaps, total = 0, 0
  for _ = 1, 60 do -- a second of full right reins
    local xin = xrplay.tick(p, state(nil, nil, right), DT, still_carpet())
    test.expect_eq(xin.turn, 0, "no smooth yaw")
    if xin.snap then
      snaps = snaps + 1
      total = total + xin.snap
      test.expect(p.vignette > 0.45, "the vignette blinks on a snap")
    end
  end
  test.expect_eq(snaps, 2, "one now, one after the cooldown")
  test.expect_near(total, math.rad(60), 1e-9)
  -- Head steering snaps too, to the left.
  local h = xrplay.new({ steer = "head", snap_turn = true })
  xrplay.tick(h, state(), DT)
  local xin = xrplay.tick(h, state(axis(0, 1, 0, 50)), DT)
  test.expect_near(xin.snap, -math.rad(30), 1e-9)
  -- The carpet turns at once by the snap.
  local c = carpet.new({ x = 0, y = 0, alt = 100 }, 0)
  local env = { move = function(pos, heading) return pos, heading end, height = function() return 0 end }
  carpet.update(c, { snap = math.rad(30) }, DT, env)
  test.expect_near(c.heading, math.rad(30), 1e-9)
  carpet.update(c, { snap = "far" }, DT, env) -- (nonsense from the network is ignored)
  test.expect_near(c.heading, math.rad(30), 1e-9)
end)

-- spec: webxr-play: Reduced speed
test.case("reduced-speed mode lowers the top speed", function()
  local p = xrplay.new({ reduced_speed = true })
  local xin = xrplay.tick(p, state(), DT)
  test.expect_eq(xin.scale, 0.5)
  test.expect_eq(xrplay.tick(xrplay.new(), state(), DT).scale, nil, "off by default")
  local env = { move = function(pos, heading) return pos, heading end, height = function() return 0 end }
  local slow, fast = carpet.new({ x = 0, y = 0, alt = 100 }, 0), carpet.new({ x = 0, y = 0, alt = 100 }, 0)
  for _ = 1, 600 do
    carpet.update(slow, { throttle = 1, scale = xin.scale }, DT, env)
    carpet.update(fast, { throttle = 1 }, DT, env)
  end
  test.expect_near(slow.speed, carpet.CRUISE_MAX * 0.5, 1e-6)
  test.expect_near(fast.speed, carpet.CRUISE_MAX, 1e-6)
end)

-- spec: webxr-play: Comfort vignette
test.case("the comfort vignette follows turning and acceleration", function()
  local p = xrplay.new()
  local c = { heading = 0, speed = 0 }
  for _ = 1, 30 do xrplay.tick(p, state(), DT, c) end
  test.expect_eq(p.vignette, 0, "still: no vignette")
  for _ = 1, 60 do
    c = { heading = c.heading + math.rad(60) * DT, speed = 20 }
    xrplay.tick(p, state(), DT, c)
  end
  test.expect_near(p.vignette, 0.5, 0.01, "turning fast: the full strength")
  for _ = 1, 120 do xrplay.tick(p, state(), DT, c) end
  test.expect(p.vignette < 0.01, "it fades once the carpet flies straight")
  for _ = 1, 30 do
    c = { heading = c.heading, speed = c.speed + 10 * DT }
    xrplay.tick(p, state(), DT, c)
  end
  test.expect(p.vignette > 0.15 and p.vignette < 0.25, "speeding up: " .. p.vignette)
  local off = xrplay.new({ vignette = 0 })
  c = { heading = 0, speed = 0 }
  for _ = 1, 30 do
    c = { heading = c.heading + 0.1, speed = 0 }
    xrplay.tick(off, state(), DT, c)
  end
  test.expect_eq(off.vignette, 0, "switched off")
  xrplay.tick(p, { active = false }, DT, c)
  test.expect_eq(p.vignette, 0, "no vignette without a session")
end)

-- spec: webxr-play: Horizon stabiliser
test.case("the seat stays level with the stabiliser", function()
  local c = { heading = math.rad(90), bank = -30, pitch = math.rad(20) }
  local level = xrplay.new()
  local cam, axes = xrplay.seat(level, c, { 10, 5, 0 }, { 0, 1, 0 })
  test.expect_near(cam.eye[2], 5 + xrplay.SEAT, 1e-9)
  test.expect_eq(cam.up[2], 1)
  test.expect_near(axes.forward[1], 1, 1e-9, "looking along the heading (east)")
  test.expect_near(cam.target[2], cam.eye[2], 1e-9, "level")
  local free = xrplay.new({ stabilise = false })
  cam, axes = xrplay.seat(free, c, { 10, 5, 0 }, { 0, 1, 0 })
  test.expect(cam.target[2] > cam.eye[2] + 3, "pitching with the carpet")
  test.expect(axes.up[2] < 0.9, "banking with the carpet")
  test.expect(axes.up[3] > 0.3, "rolled to the right (south) in a right turn")
end)

-- Points of a form's template drawn in the air, on the plane facing a head
-- with orientation q, 40 cm wide, starting 50 cm ahead of it.
local function draw_in_air(form, q)
  local t = gesture.template(form, 48)
  local right, up, fwd = xrplay.rotate(q, { 1, 0, 0 }), xrplay.rotate(q, { 0, 1, 0 }), xrplay.rotate(q, { 0, 0, -1 })
  local pts = {}
  for i = 1, #t // 2 do
    local x, y = t[2 * i - 1] * 0.2, -t[2 * i] * 0.2 -- (template y is down)
    pts[i] = { fwd[1] * 0.5 + right[1] * x + up[1] * y, fwd[2] * 0.5 + right[2] * x + up[2] * y,
      fwd[3] * 0.5 + right[3] * x + up[3] * y }
  end
  return pts
end

-- spec: webxr-play: Gestures in the air
test.case("circles and lines drawn in the air are recognised from any head orientation", function()
  local heads = { IDENTITY, axis(0, 1, 0, 90), axis(0, 1, 0, -135), axis(1, 0, 0, 30), qmul(axis(0, 1, 0, 40),
    axis(1, 0, 0, -25)) }
  for k, q in ipairs(heads) do
    for _, form in ipairs({ "circle", "line" }) do
      local p = xrplay.new()
      xrplay.tick(p, state(q), DT)
      local got
      for _, pt in ipairs(draw_in_air(form, q)) do
        local hand = controller(IDENTITY, { trigger = 1 })
        hand.pose.position = pt
        local xin = xrplay.tick(p, state(q, nil, hand), DT)
        test.expect(xin.casts.right.held)
      end
      local xin = xrplay.tick(p, state(q, nil, controller(IDENTITY, { trigger = 0.2 })), DT)
      test.expect(xin.casts.right.released)
      got = xin.gestures[1]
      test.expect(got ~= nil and got.hand == "right", "a gesture of the right hand")
      test.expect_eq(got and got.gesture.form, form, "head " .. k)
      test.expect(got and got.gesture.accuracy > 0.8, "accurate from head " .. k)
    end
  end
  -- The projection: millimetres from the first point, y down.
  local flat = xrplay.project({ { 1, 2, 3 }, { 1.1, 1.95, 2 } }, { 1, 0, 0 }, { 0, 1, 0 })
  test.expect_eq(#flat, 4)
  test.expect_near(flat[3], 100, 1e-6)
  test.expect_near(flat[4], 50, 1e-6)
  -- A trigger pressed and let go without moving is a tap; one never tracked draws nothing.
  local p = xrplay.new()
  xrplay.tick(p, state(nil, nil, controller(IDENTITY, { trigger = 1 })), DT)
  local xin = xrplay.tick(p, state(nil, nil, controller(IDENTITY)), DT)
  test.expect_eq(xin.gestures[1].gesture.form, "tap")
  xrplay.tick(p, state(nil, controller(nil, { trigger = 0.9 })), DT)
  xin = xrplay.tick(p, state(nil, controller(nil, { trigger = 0.5 })), DT)
  test.expect(xin.casts.left.held, "held until the trigger falls below the release threshold")
  xin = xrplay.tick(p, state(nil, controller(nil)), DT)
  test.expect_eq(#xin.gestures, 0)
end)

-- spec: webxr-play: Casting by drawing in the air
test.case("a gesture drawn by a hand shapes that hand's charge", function()
  local p = magic.new()
  local spell = { cast_left = { held = true, pressed = true } }
  local xin = { casts = { right = { held = true, pressed = true }, left = { held = false } },
    gestures = { { hand = "right", gesture = { form = "line", accuracy = 0.9 } } } }
  local given = {}
  xrplay.spell(xin, spell, function(g, hand) given[#given + 1] = { g, hand } end)
  test.expect(spell.cast_right.held and spell.cast_right.pressed, "the trigger is the hand's cast button")
  test.expect(spell.cast_left.held, "other devices keep theirs")
  test.expect_eq(#given, 1)
  test.expect_eq(given[1][2], "right")
  -- The magic: a gesture of the charging hand shapes the charge; another hand's makes the free form.
  p.charging = { hand = "right", seconds = 0, power = 0 }
  magic.gesture(p, { form = "tap", accuracy = 0.7 }, "right")
  test.expect_eq(p.charging.gesture.form, "tap")
  magic.gesture(p, { form = "circle", accuracy = 0.95 }, "left")
  test.expect_eq(p.free.form, "circle")
  test.expect_eq(p.charging.gesture.form, "tap")
  p.charging = nil
  magic.gesture(p, { form = "none", accuracy = 0 }, "left")
  test.expect_eq(p.messages[#p.messages].text, "That was no form")
end)

-- spec: webxr-play: HUD readable in the headset
test.case("HUD lines wrap into a short panel", function()
  local lines = { "Speed 12 m/s, altitude 140 m above the hills of Anarvan", "Mana 40/100", "x" }
  local out = xrplay.wrap(lines, 20, 10)
  test.expect_eq(out[1].text, "Speed 12 m/s,")
  test.expect_eq(out[1].index, 1)
  test.expect_eq(out[2].text, "altitude 140 m above")
  test.expect_eq(out[#out - 1].text, "Mana 40/100")
  test.expect_eq(out[#out - 1].index, 2)
  for _, l in ipairs(out) do test.expect(#l.text <= 20, l.text) end
  test.expect_eq(#xrplay.wrap(lines, 20, 2), 2, "at most max lines")
  local long = xrplay.wrap({ string.rep("a", 25) }, 10, 5)
  test.expect_eq(#long, 3, "a word longer than the panel is cut")
  test.expect_eq(long[3].text, "aaaaa")
end)
