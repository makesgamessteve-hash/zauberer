-- SPDX-License-Identifier: MIT
-- Magic in flight (concept 3.3, 8.5): the hands hold prepared weaves from the
-- spellbook; holding a hand button charges it (mana pours in at the
-- channeling rate) and letting go casts it. Drawing the weave's form while
-- charging improves control; without it gesture assist gives fair control.
-- Free weaving picks a verb and colours by their words and takes its form
-- from a drawn gesture: the next left-hand cast is that weave, and a weave
-- cast for the first time is written into the journal and prepared.
local weave = require("magic.weave")
local mage = require("magic.mage")
local spellbook = require("magic.spellbook")
local effects = require("magic.effects")
local cast = require("magic.cast")
local stroke = require("magic.stroke")
local access = require("ui.access")

local play = {}

play.ACTIONS = { "cast_left", "cast_right", "draw", "verb", "color", "color2", "look" }
for k = 1, 8 do play.ACTIONS[#play.ACTIONS + 1] = "wheel" .. k end

local VERBS = { "create", "unmake", "move", "shape", "bind", "sense", "ward", "call" }
local FREE_TIME = 8 -- seconds a drawn free weave stays ready
local ASSIST_ACCURACY = 0.8
local MESSAGE_TIME = 4

-- The first weaves from Farrokh's books (concept 7.8): Kindle, Light and a
-- fireball, with a gust to clear smoke.
local FIRST = {
  { verb = "create", colors = { "fire" }, form = "tap" },
  { verb = "create", colors = { "light" }, form = "tap" },
  { verb = "move", colors = { "fire" }, form = "line" },
  { verb = "move", colors = { "storm" }, form = "line" },
}

local function first_book()
  local book = spellbook.new()
  for i, w in ipairs(FIRST) do
    spellbook.learn(book, { verb = w.verb, colors = w.colors, form = w.form, power = 1 }, "book")
    spellbook.prepare(book, w)
    spellbook.assign(book, i, i)
  end
  spellbook.assign(book, "left", 3)
  spellbook.assign(book, "right", 1)
  spellbook.take_news(book)
  return book
end

-- A player's magic; saved: what play.snapshot returned (or nil for a new game).
function play.new(saved)
  saved = type(saved) == "table" and saved or {}
  local m = mage.new(saved.mage or { capacity = 100, channel = 10,
    mana = { fire = 35, storm = 10, light = 20, life = 10, water = 25 } }) -- a full reservoir
  if saved.mage then
    m.strain = saved.mage.strain or 0
  end
  local book = saved.book and spellbook.restore(saved.book) or first_book()
  return {
    caster = cast.new(m, book, effects.new()),
    stroke = stroke.new({ assist = true }),
    charging = nil, -- { hand, seconds, power, accuracy }
    free = { verb = 1, color = 1, color2 = 0, form = nil, until_time = 0 },
    time = 0,
    messages = {},
    last = nil,
  }
end

function play.snapshot(p)
  return { mage = mage.snapshot(p.caster.mage), book = spellbook.snapshot(p.caster.book) }
end

-- What the determinism checks hash: the spellbook. The mage's mana and
-- attunement follow the mana field where the carpet is, which streams in on
-- its own time, so they are saved but not hashed.
function play.hash_state(p)
  return { book = spellbook.snapshot(p.caster.book) }
end

local function say(p, text)
  p.messages[#p.messages + 1] = { text = text, until_time = p.time + MESSAGE_TIME }
end

-- The free weave as it stands (verb and colours by their words, the drawn form).
function play.free_weave(p, power)
  local colors = { mage.COLORS[p.free.color] }
  if p.free.color2 > 0 and p.free.color2 ~= p.free.color then colors[2] = mage.COLORS[p.free.color2] end
  return { verb = VERBS[p.free.verb], colors = colors, form = p.free.form, power = power or 1 }
end

-- The weave a hand casts now: the free weave if one was just drawn (left
-- hand), else the hand's prepared weave.
function play.hand_weave(p, hand)
  if hand == "left" and p.free.form and p.time < p.free.until_time then
    return play.free_weave(p), true
  end
  local w = spellbook.in_slot(p.caster.book, hand)
  return w, false
end

local function release(p, carpet, aim)
  local c = p.charging
  p.charging = nil
  local w, free = play.hand_weave(p, c.hand)
  if not w or c.power <= 0 then return nil end
  local spec = { verb = w.verb, colors = w.colors, form = w.form, modifiers = w.modifiers, power = c.power }
  local accuracy = ASSIST_ACCURACY
  if c.gesture then -- drawn while charging: its accuracy counts, if it is the weave's form
    accuracy = c.gesture.form == w.form and c.gesture.accuracy or c.gesture.accuracy * 0.5
  end
  local origin = world.move(carpet.pos, 0, 0, 0, 1)
  local r = cast.weave(p.caster, spec, origin, aim.heading, aim.pitch,
    { accuracy = accuracy, charge = c.seconds, carpet = carpet })
  p.last = r
  if r.ok then
    say(p, r.message)
    if free then
      p.free.form = nil
      local i = spellbook.prepare(p.caster.book, spec)
      if i then spellbook.assign(p.caster.book, "right", i) end
    end
  else
    say(p, r.message)
  end
  for _, news in ipairs(spellbook.take_news(p.caster.book)) do say(p, news) end
  return r
end

-- A recognised gesture (gesture.recognize): it shapes the weave a hand is
-- charging (hand: "left" or "right" drew it, as in VR; nil: either), else it
-- is the form of the free weave.
function play.gesture(p, g, hand)
  if p.charging and (hand == nil or p.charging.hand == hand) then
    p.charging.gesture = g
  elseif g.form ~= "none" then
    p.free.form = g.form
    p.free.until_time = p.time + FREE_TIME
    say(p, "Form: " .. g.form .. " (" .. math.floor(g.accuracy * 100 + 0.5) .. "%)")
  else
    say(p, "That was no form")
  end
end

-- One tick. states: input action states by name (see play.ACTIONS); carpet;
-- aim: { heading, pitch }; place: the mana where the mage is. Returns the
-- result of a cast made this tick, if any.
function play.tick(p, states, dt, carpet, aim, place)
  p.time = p.time + dt
  local function st(name) return states[name] or {} end
  -- Words for free weaving.
  if st("verb").pressed then p.free.verb = p.free.verb % #VERBS + 1 end
  if st("color").pressed then p.free.color = p.free.color % #mage.COLORS + 1 end
  if st("color2").pressed then p.free.color2 = (p.free.color2 + 1) % (#mage.COLORS + 1) end
  -- The quick wheel puts a prepared weave in the right hand.
  for k = 1, 8 do
    local i = p.caster.book.wheel[k]
    if st("wheel" .. k).pressed and i then
      spellbook.assign(p.caster.book, "right", i)
      say(p, "Right hand: " .. weave.name(p.caster.book.prepared[i]))
    end
  end
  -- Drawing a gesture (mouse motion while the draw button is held).
  local look = st("look")
  p.stroke.options = access.gesture_options(p.stroke.options) -- (the player's gesture assist)
  local g = access.gesture(stroke.update(p.stroke, st("draw").held, look.x or 0, look.y or 0, 0, 0, dt))
  if g then play.gesture(p, g) end
  -- Charging and casting with the hands.
  local result
  for _, hand in ipairs({ "left", "right" }) do
    local b = st("cast_" .. hand)
    if b.pressed and not p.charging and play.hand_weave(p, hand) then
      p.charging = { hand = hand, seconds = 0, power = 0 }
    end
    if p.charging and p.charging.hand == hand then
      if b.held then
        p.charging.seconds = p.charging.seconds + dt
        p.charging.power = math.min(p.charging.power + p.caster.mage.channel * dt, 200)
      else
        result = release(p, carpet, aim)
      end
    end
  end
  cast.tick(p.caster, dt, place)
  local kept = {}
  for _, m in ipairs(p.messages) do
    if m.until_time > p.time then kept[#kept + 1] = m end
  end
  p.messages = kept
  return result
end

-- The HUD lines of the magic: mana, hands, free weave, charge, messages.
function play.hud_lines(p)
  local m = p.caster.mage
  local parts = {}
  for _, c in ipairs(mage.COLORS) do
    local amount = math.floor(m.mana[c] + 0.5)
    if amount > 0 then parts[#parts + 1] = access.colour(c) .. " " .. amount end
  end
  local lines = { "Mana " .. math.floor(mage.total(m) + 0.5) .. "/" .. m.capacity .. ": " .. table.concat(parts, ", ") }
  local function hand_name(hand)
    local w = play.hand_weave(p, hand)
    return w and weave.name(w) or "-"
  end
  lines[#lines + 1] = "Left: " .. hand_name("left") .. "   Right: " .. hand_name("right")
  local fw = play.free_weave(p)
  local shown = {}
  for i, c in ipairs(fw.colors) do shown[i] = access.colour(c) end
  lines[#lines + 1] = "Free weave: " .. fw.verb .. " " .. table.concat(shown, "+") ..
    (p.free.form and p.time < p.free.until_time and (" " .. p.free.form) or " (draw a form)")
  if p.charging then
    lines[#lines + 1] = "Charging " .. math.floor(p.charging.power + 0.5) .. " mana"
  end
  if m.strain > 30 then lines[#lines + 1] = "Strain " .. math.floor(m.strain) end
  for _, msg in ipairs(p.messages) do lines[#lines + 1] = msg.text end
  return lines
end

-- What to draw: lights and bolts in flight (positions), and the gesture trail.
function play.sights(p)
  local out = { lights = {}, bolts = {} }
  for _, l in ipairs(p.caster.effects.lights) do out.lights[#out.lights + 1] = l.pos end
  for _, b in ipairs(p.caster.effects.projectiles) do
    local s = sim.projectile(b.id)
    if s then out.bolts[#out.bolts + 1] = { pos = s.pos, color = b.plan.weave.colors[1] } end
  end
  out.trail = p.stroke.drawing and stroke.trail(p.stroke, 0, 0, 40) or {}
  return out
end

return play
