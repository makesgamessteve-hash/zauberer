-- SPDX-License-Identifier: MIT
-- The player's settings: language, text and interface, colours, controls,
-- comfort, the story and survival. They are kept with the player's data (the
-- same on every world) and applied at once when they change.
local i18n = require("ui.i18n")
local text = require("ui.text")
local access = require("ui.access")

local settings = {}

settings.KEY = "settings"

local PERCENT = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 }

-- Every setting: its default and the values it may take (in menu order).
settings.SPEC = {
  master_volume = { default = 100, values = PERCENT },
  music_volume = { default = 100, values = PERCENT },
  effects_volume = { default = 100, values = PERCENT },
  mute = { default = false, values = { false, true } },
  vsync = { default = true, values = { true, false } },
  frame_cap = { default = 0, values = { 0, 30, 60, 120 } },
  pixel_height = { default = 360, values = { 240, 360, 480, 720 } },
  brightness = { default = 1, values = { 0.75, 0.9, 1, 1.1, 1.25, 1.5 } },
  invert_look = { default = false, values = { false, true } },
  language = { default = "en" }, -- any language a package offers
  text_size = { default = 1, values = { 1, 2, 3 } },
  high_contrast = { default = false, values = { false, true } },
  colour_vision = { default = "full", values = { "full", "deuteranopia", "protanopia", "tritanopia" } },
  colour_glyphs = { default = true, values = { true, false } },
  reduced_flashing = { default = false, values = { false, true } },
  captions = { default = true, values = { true, false } },
  toggle_hold = { default = false, values = { false, true } },
  gesture_assist = { default = "tolerant", values = { "off", "tolerant", "snap" } },
  look_curve = { default = 1, values = { 0.5, 1, 1.5, 2 } },
  look_speed = { default = 1, values = { 0.5, 0.75, 1, 1.5, 2 } },
  fov = { default = 70, values = { 60, 70, 80, 90, 100 } },
  story_markers = { default = false, values = { false, true } },
  hints = { default = true, values = { true, false } },
  survival = { default = "survivor", values = { "wanderer", "survivor", "hermit" } },
  xr_steer = { default = "reins", values = { "reins", "head" } },
  xr_snap_turn = { default = false, values = { false, true } },
  xr_vignette = { default = 0.5, values = { 0, 0.25, 0.5, 0.75 } },
  xr_reduced_speed = { default = false, values = { false, true } },
  xr_stabiliser = { default = true, values = { true, false } },
}

-- The menu order of the settings.
settings.ORDER = { "language", "master_volume", "music_volume", "effects_volume", "mute", "vsync", "frame_cap",
  "pixel_height", "brightness", "invert_look", "text_size", "high_contrast", "colour_vision", "colour_glyphs",
  "reduced_flashing",
  "captions", "toggle_hold", "gesture_assist", "look_curve", "look_speed", "fov",
  "story_markers", "hints", "survival", "xr_steer", "xr_snap_turn", "xr_vignette", "xr_reduced_speed",
  "xr_stabiliser" }

local function valid(name, value)
  local spec = settings.SPEC[name]
  if not spec then return false end
  if name == "language" then return type(value) == "string" end
  for _, v in ipairs(spec.values) do
    if v == value then return true end
  end
  return false
end

settings.listeners = {} -- fn(s, name) called after a setting changed

-- Applies what other modules read directly (language, text, accessibility,
-- flashes, sound, the picture and its pace).
local function apply(s)
  i18n.set(s.language)
  text.configure({ scale = s.text_size, contrast = s.high_contrast })
  access.configure(s)
  engine.set_flash_strength(s.reduced_flashing and 0.2 or 1)
  settings.apply_volume(s)
  audio.group_volume("music", s.music_volume / 100)
  audio.group_volume("effects", s.effects_volume / 100)
  engine.set_vsync(s.vsync)
  engine.set_frame_cap(s.frame_cap)
  render.internal_height(s.pixel_height)
  render.brightness(s.brightness)
end

-- The master volume as set (silent when muted, or while the game is out of focus).
function settings.apply_volume(s)
  audio.group_volume("master", (s.mute or settings.out_of_focus) and 0 or s.master_volume / 100)
end

-- The settings: saved ones over the defaults (unknown or invalid values are
-- dropped). The first start takes the system's language when a package offers it.
function settings.load(system_language)
  local saved = save.get(settings.KEY)
  local s = {}
  for name, spec in pairs(settings.SPEC) do s[name] = spec.default end
  if type(saved) == "table" then
    for name, value in pairs(saved) do
      if valid(name, value) then s[name] = value end
    end
  elseif system_language then
    for _, l in ipairs(i18n.languages()) do
      if l.code == system_language then s.language = l.code end
    end
  end
  apply(s)
  return s
end

-- Changes one setting (invalid values are refused), applies and keeps it.
function settings.set(s, name, value)
  if not valid(name, value) then return false end
  s[name] = value
  apply(s)
  for _, fn in ipairs(settings.listeners) do fn(s, name) end
  local out = {}
  for k in pairs(settings.SPEC) do out[k] = s[k] end
  save.set(settings.KEY, out)
  save.flush()
  return true
end

-- Every setting back to its default, but the language.
function settings.reset(s)
  for name, spec in pairs(settings.SPEC) do
    if name ~= "language" then s[name] = spec.default end
  end
  return settings.set(s, "language", s.language)
end

-- The next (step 1) or previous (step -1) value of a setting, wrapping round.
function settings.step(s, name, step)
  local values = settings.SPEC[name].values
  if name == "language" then
    values = {}
    for _, l in ipairs(i18n.languages()) do values[#values + 1] = l.code end
  end
  local at = 1
  for i, v in ipairs(values) do
    if v == s[name] then at = i end
  end
  return settings.set(s, name, values[(at - 1 + step) % #values + 1])
end

return settings
