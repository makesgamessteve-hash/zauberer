-- SPDX-License-Identifier: MIT
-- Accessibility in play: the player's input shaped to their settings (toggle
-- instead of hold, look curve and speed, gesture assist), captions for what
-- can be heard, glyphs beside every mana colour, markers to the story's next
-- place, survival presets and calmer camera and light.
local access = {}

-- Actions held for a while that can be switched on and off with a press instead.
access.HOLDS = { cast_left = true, cast_right = true, draw = true, channel = true, boost = true }
-- Actions the look curve and speed shape.
access.LOOKS = { look = true }

-- Every mana colour's glyph (shown beside its name, so colour is never the only cue).
access.GLYPHS = { fire = "*", storm = "~", light = "^", life = "+", water = "=", earth = "#", void = "@",
  decay = "%" }

local opts = { colour_vision = "full", toggle_hold = false, look_curve = 1, look_speed = 1, gesture_assist = "tolerant",
  colour_glyphs = true,
  reduced_flashing = false, captions = true,
  story_markers = false, fov = 70 }

function access.configure(s)
  for k in pairs(opts) do
    if s[k] ~= nil then opts[k] = s[k] end
  end
end

function access.option(name) return opts[name] end

-- ---- input ----------------------------------------------------------------

local latched, seen_tick, blocked = {}, {}, false

local IDLE = { pressed = false, released = false, held = false, x = 0, y = 0 }

local function idle() return { pressed = false, released = false, held = false, x = 0, y = 0 } end

-- While blocked (a menu is open) the game's actions read as idle.
function access.block(on) blocked = on and true or false end
function access.blocked() return blocked end

local function shaped(v)
  if v == 0 then return 0 end
  local a = math.abs(v) ^ opts.look_curve * opts.look_speed
  return v < 0 and -a or a
end

-- One action's state as the player's settings shape it. raw: the engine's
-- state; tick: the current tick (a toggle changes once per tick however often
-- it is read). menu_action: read even while the menu blocks input.
function access.shape(name, raw, tick, menu_action)
  if blocked and not menu_action then return idle() end
  local st = { pressed = raw.pressed, released = raw.released, held = raw.held, x = raw.x or 0, y = raw.y or 0 }
  if opts.toggle_hold and access.HOLDS[name] then
    local seen = seen_tick[name]
    if not seen or seen.tick ~= tick then
      local was = latched[name] or false
      local now = was
      if raw.pressed then now = not was end
      latched[name] = now
      seen = { tick = tick, held = now, pressed = now and not was, released = was and not now }
      seen_tick[name] = seen
    end
    st.held, st.pressed, st.released = seen.held, seen.pressed, seen.released
  end
  if access.LOOKS[name] then
    st.x, st.y = shaped(st.x), shaped(st.y)
  end
  return st
end

-- Wraps input.state so every reader gets the shaped states. menu_actions:
-- the actions the menu itself reads.
function access.wrap_input(inp, menu_actions)
  local raw_state = inp.state
  inp.raw_state = raw_state
  inp.state = function(name)
    return access.shape(name, raw_state(name) or IDLE, engine.tick(), menu_actions[name])
  end
end

-- Gesture recognition options for the magic's strokes.
function access.gesture_options(base)
  local o = {}
  for k, v in pairs(base or {}) do o[k] = v end
  o.assist = opts.gesture_assist ~= "off"
  return o
end

-- A recognised gesture: with "snap", any form recognised at all is held to be
-- drawn well enough (accuracy at least 0.75).
function access.gesture(g)
  if g and opts.gesture_assist == "snap" and g.form ~= "none" and g.accuracy < 0.75 then
    g.accuracy = 0.75
  end
  return g
end

-- ---- colours, light and camera -------------------------------------------

-- The retro palette for the player's colour vision: a lookup table corrected
-- for deuteranopia, protanopia or tritanopia (tools/gen/gen_palette.py).
function access.palette()
  if opts.colour_vision and opts.colour_vision ~= "full" then
    return { lut = "palettes/base_lut_" .. opts.colour_vision .. ".png" }
  end
  return {}
end

-- A colour's name with its glyph, when glyphs are on: "*fire".
function access.colour(name)
  local g = opts.colour_glyphs and access.GLYPHS[name]
  return g and (g .. name) or name
end

-- How bright a flash may be (lightning, spells): 0 .. 1.
function access.flash(strength)
  if opts.reduced_flashing then return math.min(strength, 0.2) end
  return strength
end

function access.fov(default)
  return opts.fov or default
end

-- ---- captions and markers --------------------------------------------------

access.CAPTION_TIME = 4 -- seconds a caption stays

-- Captions: what can be heard, with where it comes from. cues: a list of
-- { text, pos? }; heading: the way the player faces (radians, 0 = north,
-- clockwise); bearing(pos): the way to pos in the same measure.
function access.captions_new() return { list = {} } end

function access.caption(c, text, now, side)
  if not opts.captions then return end
  for _, e in ipairs(c.list) do
    if e.text == text and e.side == side then
      e.until_time = now + access.CAPTION_TIME
      return
    end
  end
  c.list[#c.list + 1] = { text = text, side = side, until_time = now + access.CAPTION_TIME }
end

-- The side a sound comes from: "left", "right", "ahead" or "behind".
function access.side(heading, bearing)
  local d = (bearing - heading + math.pi) % (2 * math.pi) - math.pi
  if math.abs(d) < math.pi / 4 then return "ahead" end
  if math.abs(d) > 3 * math.pi / 4 then return "behind" end
  return d > 0 and "right" or "left"
end

function access.caption_lines(c, now)
  local keep, lines = {}, {}
  for _, e in ipairs(c.list) do
    if e.until_time > now then
      keep[#keep + 1] = e
      lines[#lines + 1] = e.side and string.format("[%s, %s]", e.text, e.side) or string.format("[%s]", e.text)
    end
  end
  c.list = keep
  return lines
end

access.LISTEN_EVERY = 0.5 -- seconds between listening for sounds worth a caption
access.CALLS = { wolf = "wolves howling", boar = "a creature growls", storm_bird = "wings beating" }

-- Listens for what is worth a caption. ctx: { heading, strikes (lightning so
-- far), fires (bearings of fire close by), creatures ({ species, bearing,
-- alive }) }.
function access.listen(c, now, ctx)
  if not opts.captions or (c.next_listen or 0) > now then return end
  c.next_listen = now + access.LISTEN_EVERY
  if c.strikes and ctx.strikes and ctx.strikes > c.strikes then access.caption(c, "thunder", now) end
  c.strikes = ctx.strikes
  for _, b in ipairs(ctx.fires or {}) do access.caption(c, "fire crackling", now, access.side(ctx.heading, b)) end
  for _, cr in ipairs(ctx.creatures or {}) do
    local call = cr.alive and access.CALLS[cr.species]
    if call then access.caption(c, call, now, access.side(ctx.heading, cr.bearing)) end
  end
end

-- A marker to the story's next place (when markers are on): "Marker: Khavarpur, 3 km to the north".
function access.marker(place, directions)
  if not opts.story_markers or not place then return nil end
  return string.format("Marker: %s, %s", place, directions)
end

return access
