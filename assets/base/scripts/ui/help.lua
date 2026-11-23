-- SPDX-License-Identifier: MIT
-- Help: prompts that name the controls of the device in hand (keyboard and
-- mouse, gamepad or touch), first-time hints shown once each, and the help
-- pages (controls, flying, walking, casting, survival, the bag and tent, the
-- story so far). Text is English with {action} for a control; it is
-- translated, then the controls are put in, so the lines come out raw.
local i18n = require("ui.i18n")

local help = {}

help.KEY = "hints_seen"  -- the hints seen, kept with the player's data
help.SHOWN = 6           -- seconds a hint stays

-- What the controls are called, by binding.
local KEYS = { left_shift = "Left Shift", right_shift = "Right Shift", escape = "Escape", ["return"] = "Enter",
  space = "Space", tab = "Tab", up = "Up", down = "Down", left = "Left", right = "Right" }
local MOUSE = { left = "Left mouse button", right = "Right mouse button", middle = "Middle mouse button" }
local PAD = { a = "A", b = "B", x = "X", y = "Y", left_shoulder = "LB", right_shoulder = "RB", left_trigger = "LT",
  right_trigger = "RT", start = "Start", back = "Back", left_stick = "Left stick", right_stick = "Right stick",
  dpad_up = "D-pad up", dpad_down = "D-pad down", dpad_left = "D-pad left", dpad_right = "D-pad right" }

local function key_name(k)
  if KEYS[k] then return i18n.t(KEYS[k]) end
  return k:upper()
end

-- One binding as the player sees it (nil when it is not of the device).
function help.binding_name(b, device)
  local kind, rest = b:match("^([%a_]+):(.*)$")
  if device == "desktop" then
    if kind == "key" then return key_name(rest) end
    if kind == "keys" then
      local parts = {}
      for k in rest:gmatch("[^,]+") do parts[#parts + 1] = key_name(k) end
      return table.concat(parts, " ")
    end
    if kind == "mouse" then return i18n.t(MOUSE[rest] or rest) end
  elseif device == "gamepad" and kind == "gamepad" then
    local name = rest:gsub("^%-", "")
    return PAD[name] and i18n.t(PAD[name]) or name
  elseif device == "touch" and kind == "touch" then
    local what, name = rest:match("^(%a+):(.+)$")
    local label = require("flight.controls").LABELS[name] or (name:sub(1, 1):upper() .. name:sub(2))
    return what == "stick" and ("(" .. i18n.t(label) .. ")") or ("[" .. i18n.t(label) .. "]")
  end
  return nil
end

-- The control of an action on a device (the device in hand by default).
function help.control(action, device)
  device = device or input.device()
  for _, b in ipairs(input.bindings(action) or {}) do
    local name = help.binding_name(b, device)
    if name then return name end
  end
  return i18n.t("not set")
end

-- A line of help: translated, with each {action} replaced by its control.
function help.fill(textline, device)
  local shown = i18n.t(textline)
  return (shown:gsub("{([%w_]+)}", function(action) return help.control(action, device) end))
end

-- ---- first-time hints ------------------------------------------------------------------

-- Each hint: when it applies (ctx: flying, low_slow, on_foot, near_carpet,
-- near_use, can_cast, has_bag, tired, hungry) and what it says.
help.HINTS = {
  { id = "fly", when = function(c) return c.flying end,
    text = "Look where to fly and hold {steer} forward; let go to slow down. "
      .. "Climb and dive with {climb}, boost with {boost}." },
  { id = "menu", when = function(c) return c.flying end, text = "Menu, help and saving: {quit}." },
  { id = "step_off", when = function(c) return c.low_slow end, text = "Step off the carpet with {mount}." },
  { id = "board", when = function(c) return c.on_foot and c.near_carpet end, text = "Get on the carpet with {mount}." },
  { id = "use", when = function(c) return c.on_foot and c.near_use end, text = "Use, open and talk with {use}." },
  { id = "cast", when = function(c) return c.can_cast end,
    text = "Cast with {cast_left} and {cast_right}; hold {draw} and draw a form for a free weave." },
  { id = "bag", when = function(c) return c.has_bag end,
    text = "Stow in the endless bag with {stow}, take out with {unstow}." },
  { id = "tired", when = function(c) return c.tired and c.on_foot end, text = "Tired: sleep with {sleep}." },
  { id = "hungry", when = function(c) return c.hungry end,
    text = "Hungry or thirsty: eat with {eat}, drink with {drink}." },
}

function help.new()
  local saved = save.get(help.KEY)
  local seen = {}
  for _, id in ipairs(type(saved) == "table" and saved or {}) do seen[id] = true end
  return { seen = seen, showing = nil, until_time = 0 }
end

local function keep(h)
  local list = {}
  for id in pairs(h.seen) do list[#list + 1] = id end
  table.sort(list)
  save.set(help.KEY, list)
  save.flush()
end

-- One tick (now: seconds; on: hints turned on). Returns the hint showing, if any.
function help.tick(h, now, ctx, on)
  if h.showing and now < h.until_time then return h.showing end
  h.showing = nil
  if not on then return nil end
  for _, hint in ipairs(help.HINTS) do
    if not h.seen[hint.id] and hint.when(ctx) then
      h.seen[hint.id] = true
      keep(h)
      h.showing, h.until_time = hint, now + help.SHOWN
      return hint
    end
  end
  return nil
end

-- The HUD line of the hint showing.
function help.hint_line(h)
  return h.showing and { raw = help.fill(h.showing.text) } or nil
end

-- Shows every hint again from the start.
function help.reset(h)
  h.seen, h.showing = {}, nil
  keep(h)
end

-- ---- the help pages ------------------------------------------------------------------------

-- The controls, as the controls page lists them.
help.ACTIONS = {
  { "steer", "Steer, walk" }, { "climb", "Climb and dive" }, { "boost", "Boost, run" }, { "mount", "Step off, get on" },
  { "jump", "Jump" }, { "use", "Use, open, talk" }, { "cast_left", "Cast with the left hand" },
  { "cast_right", "Cast with the right hand" }, { "draw", "Draw a form" }, { "verb", "Next verb" },
  { "color", "Next colour" }, { "channel", "Channel mana" }, { "sleep", "Sleep" }, { "gather", "Gather" },
  { "campfire", "Lay a campfire" }, { "cook", "Cook" }, { "eat", "Eat" }, { "drink", "Drink" },
  { "stow", "Stow in the bag" }, { "unstow", "Take out of the bag" }, { "cloak", "Cloak" }, { "tent", "Tent" },
  { "codex", "Codex" }, { "story", "Begin the story" }, { "quit", "Menu" },
}

help.TOPICS = {
  { id = "controls", title = "Controls" },
  { id = "flying", title = "Flying", lines = {
    "Look where to fly and hold {steer} forward; let go to slow down. Climb and dive with {climb}, boost with {boost}.",
    "Low and slow over the ground, step off with {mount}.",
    "Wet, torn or burnt, the carpet flies worse: patch it on the ground.",
    "The carpet stays where you leave it; get on again with {mount} beside it." } },
  { id = "walking", title = "Walking", lines = {
    "Walk with {steer}, run with {boost}, jump with {jump}.",
    "Use, open doors and talk to people with {use}.",
    "Sleep with {sleep}: the night passes and you are rested.",
    "Deep water stops you: fly over it, or freeze it." } },
  { id = "casting", title = "Casting", lines = {
    "Each hand holds a prepared weave: cast it with {cast_left} or {cast_right}.",
    "Hold {draw} and draw a form for a free weave: tap, line, circle, spiral, triangle, square, zigzag or wave.",
    "Choose the verb with {verb} and the colour with {color}.",
    "Channel mana from the land with {channel}; mana motes from creatures fill you too." } },
  { id = "survival", title = "Survival", lines = {
    "Food, water, rest and warmth run down: eat with {eat}, drink with {drink}.",
    "Gather with {gather}, lay a campfire with {campfire} and cook with {cook}.",
    "Boil water before you drink it; the Codex ({codex}) warns of what is poisonous.",
    "Fire burns: get out of it. Defeated, you get up again where the carpet set out." } },
  { id = "bag", title = "Bag and tent", lines = {
    "The endless bag holds folds, not weight: stow with {stow}, take out with {unstow}.",
    "Pitch the carpet as a tent with {tent} and go in with {mount}.",
    "Wear the carpet as a cloak with {cloak}." } },
  { id = "story", title = "The story so far" },
}

-- The lines of a topic (story: the story's own lines, given).
function help.topic_lines(id, story_lines)
  local out = {}
  if id == "controls" then
    for _, a in ipairs(help.ACTIONS) do
      out[#out + 1] = { raw = i18n.t(a[2]) .. ": " .. help.control(a[1]) }
    end
    return out
  end
  if id == "story" then
    for _, l in ipairs(story_lines or {}) do out[#out + 1] = l end
    if #out == 0 then out[1] = "The story has not begun: press {story} in Anarvan." end
    for i, l in ipairs(out) do
      if type(l) == "string" and l:find("{", 1, true) then out[i] = { raw = help.fill(l) } end
    end
    return out
  end
  for _, t in ipairs(help.TOPICS) do
    if t.id == id then
      for _, l in ipairs(t.lines or {}) do out[#out + 1] = { raw = help.fill(l) } end
    end
  end
  return out
end

return help
