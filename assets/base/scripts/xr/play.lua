-- SPDX-License-Identifier: MIT
-- Immersive VR play (concept 8.5, "WebXR"; openspec add-webxr-play): flying
-- the carpet with the controllers, casting by drawing gestures in the air,
-- comfort options and a HUD panel that can be read in the headset.
--
-- Steering: tilting a controller like reins (roll turns, pitch climbs) or
-- looking where to fly (steer = "head"), relative to a neutral pose taken
-- when the session starts and again after a grip squeeze held for a second.
-- The left thumbstick's y is the throttle and its x adds turning; the right
-- thumbstick's y adds climbing. The result adds up with the other devices'
-- flight input (play.flight).
--
-- Gestures: while a hand's trigger is held its controller's path is
-- recorded; on release it is projected onto the plane facing the player at
-- the stroke's start (head right and up), in millimetres from its first point
-- with y down as on a screen, and recognised. The trigger is that hand's cast
-- button too, so a weave charged with a hand takes its form from the gesture
-- the same hand draws, as G-drawing does on the desktop (play.spell).
--
-- Everything here is a pure function of the states passed in, except
-- play.frame and play.draw_hud, which draw.
local M = {}

M.DEFAULTS = {
  steer = "reins", -- "reins" (tilt a controller) or "head" (look where to fly)
  rein_hand = "right", -- the hand holding the reins (the other one when it is not tracked)
  snap_turn = false, -- turn in steps instead of smoothly
  snap_angle = math.rad(30),
  snap_cooldown = 0.5, -- seconds between snaps
  vignette = 0.5, -- strength of the comfort vignette while turning or speeding up (0: off)
  stabilise = true, -- the seat stays level while the carpet banks and pitches
  reduced_speed = false,
  speed_scale = 0.5, -- top speed in reduced-speed mode
}

M.SEAT = 1.1 -- metres from the carpet's position up to the session origin
M.TRIGGER_ON, M.TRIGGER_OFF = 0.6, 0.35 -- a stroke starts and ends (hysteresis)
M.SQUEEZE_ON = 0.8
M.RECENTRE_TIME = 1 -- seconds of squeezing that recentre
M.STICK_DEAD = 0.15
M.REINS = { roll_dead = math.rad(5), roll_full = math.rad(40), pitch_dead = math.rad(5), pitch_full = math.rad(30) }
M.LOOK = { yaw_dead = math.rad(10), yaw_full = math.rad(45), pitch_dead = math.rad(10), pitch_full = math.rad(35) }
M.SNAP_AT = 0.6 -- turn input that snaps
M.MAX_POINTS = 1024 -- per stroke
M.GESTURE = { assist = true, tap_size = 30 } -- (millimetres)
M.TURN_FULL = math.rad(40) -- yaw rate (per second) of the full vignette
M.ACCEL_FULL = 25 -- m/s^2 of the full vignette
M.HUD = { distance = 1.5, pixel = 0.004, chars = 36, lines = 10, top = 50, advance = 6, line_height = 9 }

local HANDS = { "left", "right" }

local function clamp(v, lo, hi)
  if v < lo then return lo end
  if v > hi then return hi end
  return v
end

local function wrap_angle(a)
  return (a + math.pi) % (2 * math.pi) - math.pi
end

-- ---- Rotations (quaternions { x, y, z, w }) --------------------------------

local function qmul(a, b)
  return {
    a[4] * b[1] + a[1] * b[4] + a[2] * b[3] - a[3] * b[2],
    a[4] * b[2] - a[1] * b[3] + a[2] * b[4] + a[3] * b[1],
    a[4] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[4],
    a[4] * b[4] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3],
  }
end

local function qconj(q)
  return { -q[1], -q[2], -q[3], q[4] }
end

-- v rotated by the unit quaternion q.
function M.rotate(q, v)
  local x, y, z, w = q[1], q[2], q[3], q[4]
  local tx, ty, tz = 2 * (y * v[3] - z * v[2]), 2 * (z * v[1] - x * v[3]), 2 * (x * v[2] - y * v[1])
  return { v[1] + w * tx + (y * tz - z * ty), v[2] + w * ty + (z * tx - x * tz), v[3] + w * tz + (x * ty - y * tx) }
end

local function copy(t)
  local out = {}
  for i, v in ipairs(t) do out[i] = v end
  return out
end

local function dot(a, b)
  return a[1] * b[1] + a[2] * b[2] + a[3] * b[3]
end

-- How the orientation q is tilted from the neutral q0, in the neutral's own
-- axes (x right, y up, -z forward): pitch (nose up +), roll (right side down
-- +) and yaw (to the right +), in radians.
function M.tilt(q0, q)
  local rel = qmul(qconj(q0), q)
  local f, r, u = M.rotate(rel, { 0, 0, -1 }), M.rotate(rel, { 1, 0, 0 }), M.rotate(rel, { 0, 1, 0 })
  return math.asin(clamp(f[2], -1, 1)), math.atan(-r[2], u[2]), math.atan(f[1], -f[3])
end

-- An angle past a dead zone as -1 .. 1 (full at `full`).
local function shape(a, dead, full)
  local m = clamp((math.abs(a) - dead) / (full - dead), 0, 1)
  return a < 0 and -m or m
end

local function stick(v)
  return math.abs(v or 0) < M.STICK_DEAD and 0 or v
end

-- ---- State -----------------------------------------------------------------

-- The VR play state; options: overrides of M.DEFAULTS (a settings menu's).
function M.new(options)
  local o = {}
  for k, v in pairs(M.DEFAULTS) do o[k] = v end
  for k, v in pairs(options or {}) do o[k] = v end
  return { options = o, active = false, neutral = nil, squeeze_time = 0, recentred = false, snap_wait = 0,
    vignette = 0, last = nil, hands = { left = { held = false }, right = { held = false } } }
end

-- Takes the head's and the tracked controllers' orientations as the neutral pose.
function M.recentre(p, xs)
  local n = { head = copy(xs.head.orientation) }
  for _, hand in ipairs(HANDS) do
    local c = xs.controllers and xs.controllers[hand]
    if c and c.tracked then n[hand] = copy(c.pose.orientation) end
  end
  p.neutral = n
end

-- Projects a 3D path ({ {x, y, z}, ... } in the session's space) onto the
-- plane with the axes right and up (unit vectors): { x1, y1, ... } in
-- millimetres from the first point, y down as on a screen.
function M.project(points, right, up)
  local out, o = {}, points[1]
  for _, pt in ipairs(points) do
    local d = { pt[1] - o[1], pt[2] - o[2], pt[3] - o[3] }
    out[#out + 1] = dot(d, right) * 1000
    out[#out + 1] = -dot(d, up) * 1000
  end
  return out
end

-- One hand's trigger: its cast button and its stroke in the air.
local function hand_tick(p, hand, c, head, out)
  local h = p.hands[hand]
  local trigger = c and c.trigger or 0
  local was = h.held
  local now = (was and trigger > M.TRIGGER_OFF) or (not was and trigger >= M.TRIGGER_ON)
  h.held = now
  out.casts[hand] = { held = now, pressed = now and not was, released = was and not now }
  if now and not was then -- the plane facing the player as the stroke starts
    h.points = {}
    h.right, h.up = M.rotate(head.orientation, { 1, 0, 0 }), M.rotate(head.orientation, { 0, 1, 0 })
  end
  if now and c and c.tracked and #h.points < M.MAX_POINTS then
    h.points[#h.points + 1] = copy(c.pose.position)
  end
  if was and not now then
    if #h.points > 0 then
      local g = gesture.recognize(M.project(h.points, h.right, h.up), M.GESTURE)
      out.gestures[#out.gestures + 1] = { hand = hand, gesture = g }
    end
    h.points = nil
  end
end

-- Turn and climb from the reins or the head (-1 .. 1 each).
local function steering(p, xs)
  local o = p.options
  if o.steer == "head" then
    local pitch, _, yaw = M.tilt(p.neutral.head, xs.head.orientation)
    return shape(yaw, M.LOOK.yaw_dead, M.LOOK.yaw_full), shape(pitch, M.LOOK.pitch_dead, M.LOOK.pitch_full)
  end
  local ctl = xs.controllers or {}
  local hand = o.rein_hand
  if not (ctl[hand] and ctl[hand].tracked) then hand = hand == "right" and "left" or "right" end
  local c = ctl[hand]
  if not (c and c.tracked and p.neutral[hand]) or p.hands[hand].held then
    return 0, 0 -- (no reins, or the hand is drawing)
  end
  local pitch, roll = M.tilt(p.neutral[hand], c.pose.orientation)
  return shape(roll, M.REINS.roll_dead, M.REINS.roll_full), shape(pitch, M.REINS.pitch_dead, M.REINS.pitch_full)
end

-- The comfort vignette follows how fast the carpet turns and changes speed.
local function comfort(p, carpet, dt)
  local o, target = p.options, 0
  if carpet and p.last and dt > 0 then
    local yaw = math.abs(wrap_angle(carpet.heading - p.last.heading)) / dt
    local accel = math.abs(carpet.speed - p.last.speed) / dt
    target = o.vignette * clamp(math.max(yaw / M.TURN_FULL, accel / M.ACCEL_FULL), 0, 1)
  end
  local rate = target > p.vignette and 10 or 3
  p.vignette = p.vignette + (target - p.vignette) * math.min(1, rate * dt)
  p.last = carpet and { heading = carpet.heading, speed = carpet.speed } or nil
end

-- One tick with xs = xr.state() and the carpet as it is (for the vignette).
-- Returns nil without a session, else the VR input: { turn, throttle, climb
-- (-1 .. 1), snap (radians, a snap turn now) or nil, scale (top speed) or
-- nil, casts = { left = { held, pressed, released }, right = ... },
-- gestures = { { hand, gesture }, ... }, recentred = true on a recentre }.
function M.tick(p, xs, dt, carpet)
  if not (xs and xs.active) then
    if p.active then
      p.active = false
      p.hands = { left = { held = false }, right = { held = false } }
    end
    p.vignette, p.last = 0, nil
    return nil
  end
  local o = p.options
  if not p.active then -- a new session: a new neutral pose
    p.active, p.neutral = true, nil
  end
  if not p.neutral then M.recentre(p, xs) end
  local ctl = xs.controllers or {}
  local out = { turn = 0, throttle = 0, climb = 0, casts = {}, gestures = {} }
  local squeezing = false
  for _, hand in ipairs(HANDS) do
    local c = ctl[hand]
    if c and c.tracked and not p.neutral[hand] then p.neutral[hand] = copy(c.pose.orientation) end
    squeezing = squeezing or (c ~= nil and (c.squeeze or 0) >= M.SQUEEZE_ON)
    hand_tick(p, hand, c, xs.head, out)
  end
  if squeezing then
    p.squeeze_time = p.squeeze_time + dt
    if p.squeeze_time >= M.RECENTRE_TIME and not p.recentred then
      M.recentre(p, xs)
      p.recentred, out.recentred = true, true
    end
  else
    p.squeeze_time, p.recentred = 0, false
  end
  local turn, climb = steering(p, xs)
  local left, right = ctl.left and ctl.left.axes or {}, ctl.right and ctl.right.axes or {}
  turn = clamp(turn + stick(left[1]), -1, 1)
  climb = clamp(climb + stick(right[2]), -1, 1)
  out.throttle = stick(left[2])
  p.snap_wait = math.max(0, p.snap_wait - dt)
  if o.snap_turn then
    if math.abs(turn) >= M.SNAP_AT and p.snap_wait <= 0 then
      out.snap = turn > 0 and o.snap_angle or -o.snap_angle
      p.snap_wait = o.snap_cooldown
      p.vignette = math.max(p.vignette, o.vignette) -- (a blink hides the jump)
    end
    turn = 0
  end
  out.turn, out.climb = turn, climb
  out.scale = o.reduced_speed and o.speed_scale or nil
  comfort(p, carpet, dt)
  return out
end

-- The flight input with the VR input added: { turn, throttle, climb, boost,
-- snap, scale } for flight.carpet.
function M.flight(fin, xin)
  return { turn = clamp((fin.turn or 0) + xin.turn, -1, 1), throttle = clamp((fin.throttle or 0) + xin.throttle, -1, 1),
    climb = clamp((fin.climb or 0) + xin.climb, -1, 1), boost = fin.boost, snap = xin.snap, scale = xin.scale }
end

-- Adds the triggers to the spell states (cast_left, cast_right) and gives
-- each gesture drawn in the air to give(gesture, hand), before the magic's
-- tick.
function M.spell(xin, spell, give)
  for hand, b in pairs(xin.casts) do
    local name = "cast_" .. hand
    local s = spell[name] or {}
    spell[name] = { held = s.held or b.held, pressed = s.pressed or b.pressed, released = s.released or b.released,
      x = s.x, y = s.y }
  end
  for _, g in ipairs(xin.gestures) do give(g.gesture, g.hand) end
end

-- ---- Presentation ----------------------------------------------------------

local function add(a, b, k)
  return { a[1] + b[1] * k, a[2] + b[2] * k, a[3] + b[3] * k }
end

local function cross(a, b)
  return { a[2] * b[3] - a[3] * b[2], a[3] * b[1] - a[1] * b[3], a[1] * b[2] - a[2] * b[1] }
end

local function normalize(v)
  local l = math.sqrt(dot(v, v))
  return l > 0 and { v[1] / l, v[2] / l, v[3] / l } or v
end

-- The seat (the session origin) for render.camera: above the carpet at pos
-- (render coordinates) with up, looking along its heading; level with the
-- stabiliser, else pitching and banking with the carpet. Also returns its
-- axes { right, up, forward }.
function M.seat(p, carpet, pos, up)
  local h = carpet.heading
  local fwd = { math.sin(h), 0, -math.cos(h) }
  local right = normalize(cross(fwd, up))
  local u = up
  if not p.options.stabilise then
    local pitch, bank = carpet.pitch or 0, -math.rad(carpet.bank or 0)
    local f2 = add({ 0, 0, 0 }, fwd, math.cos(pitch))
    f2 = add(f2, up, math.sin(pitch))
    u = add(add({ 0, 0, 0 }, up, math.cos(pitch)), fwd, -math.sin(pitch))
    u = add(add({ 0, 0, 0 }, u, math.cos(bank)), right, math.sin(bank))
    fwd = f2
    right = normalize(cross(fwd, u))
  end
  local eye = add(pos, up, M.SEAT)
  return { eye = eye, target = add(eye, fwd, 10), up = u, fov = 90, near = 0.1, far = 30000 },
    { right = right, up = u, forward = fwd }
end

-- In the frame hook: the comfort vignette, and while riding (not on foot)
-- the seat as the camera and the strokes being drawn as sprites (a sprite
-- handle, optional). Without a session the vignette is switched off again.
function M.frame(p, carpet, pos, up, riding, sprite)
  if not p then return end
  if not xr.state().active then
    if p.shown then
      render.vignette(0)
      p.shown = false
    end
    return
  end
  p.shown = true
  render.vignette(p.vignette)
  if not riding then return end
  local cam, axes = M.seat(p, carpet, pos, up)
  render.camera(cam)
  if not sprite then return end
  for _, hand in ipairs(HANDS) do
    local pts = p.hands[hand].points
    for i = 1, pts and #pts or 0, 3 do -- (session space: x right, y up, -z forward of the seat)
      local q = pts[i]
      local at = add(add(add(cam.eye, axes.right, q[1]), axes.up, q[2]), axes.forward, -q[3])
      render.draw_sprite(sprite, at, 0.08)
    end
  end
end

-- HUD lines wrapped at `chars` characters (continuations keep their line's
-- index, for its colour), at most `max` of them: { { text, index }, ... }.
function M.wrap(lines, chars, max)
  local out = {}
  for i, line in ipairs(lines) do
    local rest = line
    while #out < max do
      if #rest <= chars then
        out[#out + 1] = { text = rest, index = i }
        break
      end
      local cut = rest:sub(1, chars + 1):match("^.*() ") or chars + 1
      if cut <= 1 then cut = chars + 1 end
      out[#out + 1] = { text = rest:sub(1, cut - 1), index = i }
      rest = rest:sub(cut):gsub("^ +", "")
    end
  end
  return out
end

local SHADOW = { 0, 0, 0, 1 }

-- In VR: draws the HUD lines (render.draw_text's canvas) as a panel low in
-- front of the player, on a head-locked plane (render.xr_hud) so both eyes
-- see it in the same place. color(i) gives line i's colour.
function M.draw_hud(font, lines, color)
  local H = M.HUD
  render.xr_hud(H.distance, H.pixel)
  local st = render.stats()
  local cw = st.internal_width / math.max(st.views, 1)
  local x = math.floor(cw / 2 - H.chars * H.advance / 2)
  local y = math.floor(st.internal_height / 2 + H.top)
  for k, line in ipairs(M.wrap(lines, H.chars, H.lines)) do
    local ly = y + (k - 1) * H.line_height
    render.draw_text(font, line.text, x + 1, ly + 1, 1, SHADOW) -- (readable over bright ground)
    render.draw_text(font, line.text, x, ly, 1, color(line.index))
  end
end

return M
