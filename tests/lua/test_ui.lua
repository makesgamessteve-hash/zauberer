-- SPDX-License-Identifier: MIT
-- Unit tests of the settings, accessibility in play and the menu
-- (assets/base/scripts/ui/settings.lua, access.lua, menu.lua, text.lua).
local settings = require("ui.settings")
local access = require("ui.access")
local menu = require("ui.menu")
local text = require("ui.text")
local i18n = require("ui.i18n")

local function fresh()
  save.set(settings.KEY, nil)
  return settings.load()
end

-- spec: accessibility: Settings are kept
test.case("settings: defaults, changes kept with the player's data, invalid values refused", function()
  local s = fresh()
  test.expect_eq(s.language, "en")
  test.expect_eq(s.text_size, 1)
  test.expect(not settings.set(s, "text_size", 7), "not a size")
  test.expect(not settings.set(s, "no_such_setting", true))
  test.expect(settings.set(s, "text_size", 2))
  test.expect(settings.set(s, "high_contrast", true))
  test.expect_eq(text.style().scale, 2)
  test.expect(text.style().contrast)
  local again = settings.load()
  test.expect_eq(again.text_size, 2, "kept")
  test.expect(again.high_contrast)
  -- Stepping wraps round; the language steps through the languages packages offer.
  test.expect(settings.step(again, "text_size", 1) and again.text_size == 3)
  test.expect(settings.step(again, "text_size", 1) and again.text_size == 1)
  test.expect(settings.step(again, "language", 1) and again.language == "de" and i18n.code() == "de")
  test.expect(settings.step(again, "language", -1) and again.language == "en" and i18n.code() == "en")
  -- A saved value that is no longer valid gives way to the default.
  save.set(settings.KEY, { text_size = 9, survival = "hermit" })
  local back = settings.load()
  test.expect_eq(back.text_size, 1)
  test.expect_eq(back.survival, "hermit")
  fresh()
end)

-- spec: localisation: The system's language at the first start
test.case("the first start takes the system's language when a package offers it", function()
  save.set(settings.KEY, nil)
  test.expect_eq(settings.load("de").language, "de")
  save.set(settings.KEY, nil)
  test.expect_eq(settings.load("tlh").language, "en", "no such language: English")
  save.set(settings.KEY, { language = "en" })
  test.expect_eq(settings.load("de").language, "en", "a saved choice wins")
  fresh()
end)

-- spec: accessibility: Toggle instead of hold
test.case("toggle instead of hold, look curve and speed", function()
  local s = fresh()
  local up = { pressed = false, released = false, held = false, x = 0, y = 0 }
  local press = { pressed = true, released = false, held = true, x = 0, y = 0 }
  local let_go = { pressed = false, released = true, held = false, x = 0, y = 0 }
  -- Holding as usual.
  test.expect(not access.shape("channel", let_go, 1).held)
  settings.set(s, "toggle_hold", true)
  local st = access.shape("channel", press, 10)
  test.expect(st.held and st.pressed, "a press switches it on")
  st = access.shape("channel", press, 10)
  test.expect(st.held and st.pressed, "read again in the same tick: the same")
  test.expect(access.shape("channel", let_go, 11).held, "letting go keeps it on")
  test.expect(access.shape("channel", up, 12).held)
  st = access.shape("channel", press, 13)
  test.expect(not st.held and st.released, "the next press switches it off")
  test.expect(access.shape("verb", let_go, 14).released, "other actions are as they are")
  -- The look curve and speed shape the look axes only.
  settings.set(s, "look_curve", 2)
  settings.set(s, "look_speed", 0.5)
  st = access.shape("look", { x = 0.5, y = -1 }, 15)
  test.expect(math.abs(st.x - 0.125) < 1e-9 and math.abs(st.y + 0.5) < 1e-9, st.x .. " " .. st.y)
  test.expect_eq(access.shape("steer", { x = 0.5, y = 0 }, 15).x, 0.5)
  -- While the menu is open, the game's actions are idle; the menu's are not.
  access.block(true)
  test.expect(not access.shape("cast_left", press, 16).pressed)
  test.expect(access.shape("quit", press, 16, true).pressed)
  access.block(false)
  test.expect(not access.blocked())
  fresh()
end)

-- spec: accessibility: Gesture assist
test.case("gesture assist", function()
  local s = fresh()
  local o = access.gesture_options({ tap_size = 9 })
  test.expect(o.assist and o.tap_size == 9)
  settings.set(s, "gesture_assist", "off")
  test.expect(not access.gesture_options().assist)
  test.expect_eq(access.gesture({ form = "circle", accuracy = 0.4 }).accuracy, 0.4)
  settings.set(s, "gesture_assist", "snap")
  test.expect_eq(access.gesture({ form = "circle", accuracy = 0.4 }).accuracy, 0.75)
  test.expect_eq(access.gesture({ form = "none", accuracy = 0.1 }).accuracy, 0.1)
  test.expect(access.gesture(nil) == nil)
  fresh()
end)

-- spec: accessibility: Colour is never the only cue
-- spec: accessibility: Calmer light and camera
test.case("colour glyphs, flashes and field of view", function()
  local s = fresh()
  test.expect_eq(access.colour("fire"), "*fire")
  test.expect_eq(access.colour("nothing"), "nothing")
  settings.set(s, "colour_glyphs", false)
  test.expect_eq(access.colour("fire"), "fire")
  -- spec: accessibility: Colour vision modes
  test.expect_eq(next(access.palette()), nil, "full colour vision: the plain palette")
  settings.set(s, "colour_vision", "deuteranopia")
  test.expect_eq(access.palette().lut, "palettes/base_lut_deuteranopia.png")
  test.expect(vfs.exists(access.palette().lut), "the corrected table is there")
  test.expect_eq(access.flash(1), 1)
  settings.set(s, "reduced_flashing", true)
  test.expect_eq(access.flash(1), 0.2)
  settings.set(s, "fov", 90)
  test.expect_eq(access.fov(70), 90)
  fresh()
end)

-- spec: accessibility: Sound captions
-- spec: accessibility: Story markers
test.case("captions say what is heard and where; markers only when asked for", function()
  local s = fresh()
  local c = access.captions_new()
  test.expect_eq(access.side(0, 0.1), "ahead")
  test.expect_eq(access.side(0, math.pi), "behind")
  test.expect_eq(access.side(0, math.pi / 2), "right")
  test.expect_eq(access.side(0, -math.pi / 2), "left")
  access.caption(c, "fire crackling", 10, "left")
  access.caption(c, "fire crackling", 11, "left") -- (once, for longer)
  access.caption(c, "thunder", 10)
  local lines = access.caption_lines(c, 12)
  test.expect_eq(#lines, 2)
  test.expect_eq(lines[1], "[fire crackling, left]")
  test.expect_eq(lines[2], "[thunder]")
  test.expect_eq(#access.caption_lines(c, 14.5), 1, "the thunder is over")
  settings.set(s, "captions", false)
  access.caption(c, "thunder", 20)
  test.expect_eq(#access.caption_lines(c, 20), 0)
  -- Listening: lightning since last time, fire close by, a wolf behind.
  settings.set(s, "captions", true)
  local h = access.captions_new()
  access.listen(h, 0, { heading = 0, strikes = 3 })
  access.listen(h, 0.1, { heading = 0, strikes = 4 }) -- (too soon: not heard)
  access.listen(h, 1, { heading = 0, strikes = 4, fires = { math.pi / 2 },
    creatures = { { species = "wolf", alive = true, bearing = math.pi },
      { species = "deer", alive = true, bearing = 0 }, { species = "wolf", alive = false, bearing = 0 } } })
  test.expect_eq(table.concat(access.caption_lines(h, 1), " "),
    "[thunder] [fire crackling, right] [wolves howling, behind]")
  test.expect(access.marker("Khavarpur", "a short walk to the east") == nil)
  settings.set(s, "story_markers", true)
  test.expect_eq(access.marker("Khavarpur", "a short walk to the east"), "Marker: Khavarpur, a short walk to the east")
  fresh()
end)

local function states(pressed)
  return function(name)
    local p = pressed[name]
    if type(p) == "table" then return p end
    return { pressed = p == true, held = p == true, released = false, x = 0, y = 0 }
  end
end

-- spec: accessibility: The menu
test.case("the menu: open, choose, change a setting, back and resume", function()
  local s = fresh()
  local quit, resumed, exploring = false, false, false
  local m = menu.new(s, { quit = function() quit = true end, resume = function() resumed = true end,
    explorer = function() exploring = true end,
    packages = function() return { { id = "base", version = "1", enabled = true, turned_off = false } } end,
    set_package = function(id, on) return id ~= "base" or on, "the base game cannot be turned off" end })
  menu.tick(m, states({}), 1) -- (closed: nothing happens)
  test.expect(menu.toggle(m) and m.open)
  test.expect_eq(menu.lines(m)[1], "Menu")
  test.expect(menu.lines(m)[2].raw:find("> Resume", 1, true))
  -- Down one, then to Settings (after the Map, the Journal and Help) and in.
  menu.tick(m, states({ steer = { x = 0, y = -1 } }), 1)
  test.expect_eq(m.at, 2)
  menu.tick(m, states({ steer = { x = 0, y = -1 } }), 0.1) -- (held: not yet again)
  test.expect_eq(m.at, 2)
  m.at = 5
  menu.tick(m, states({ confirm = true }), 0.1)
  test.expect_eq(m.page, "settings")
  -- Right changes the language.
  menu.tick(m, states({ steer = { x = 1, y = 0 } }), 1)
  test.expect_eq(s.language, "de")
  test.expect(menu.lines(m)[2].raw:find("Sprache: Deutsch", 1, true), menu.lines(m)[2].raw)
  settings.set(s, "language", "en")
  -- Back to the main page, then to Packages.
  menu.tick(m, states({ quit = true }), 0.1)
  test.expect_eq(m.page, "main")
  m.at = 7
  menu.tick(m, states({ confirm = true }), 0.1)
  test.expect_eq(m.page, "packages")
  test.expect(menu.lines(m)[2].raw:find("base 1: on", 1, true), menu.lines(m)[2].raw)
  menu.tick(m, states({ confirm = true }), 0.1)
  test.expect_eq(m.note, "the base game cannot be turned off")
  -- Credits.
  m.page = "credits"
  test.expect_eq(menu.lines(m, { "  someone" })[2], "  someone")
  -- Resume closes it; Save and quit quits.
  m.page, m.at = "main", 1
  menu.tick(m, states({ confirm = true }), 0.1)
  test.expect(not m.open and resumed)
  menu.toggle(m)
  m.at = 9
  menu.tick(m, states({ confirm = true }), 0.1)
  test.expect(exploring and not m.open, "the world explorer opens from the menu")
  menu.toggle(m)
  m.at = 10
  menu.tick(m, states({ confirm = true }), 0.1)
  test.expect(quit)
  fresh()
end)

-- spec: accessibility: Remapping controls
test.case("remapping a control replaces the binding of the same device", function()
  test.expect_eq(table.concat(menu.rebind({ "key:v", "gamepad:dpad_down", "touch:button:channel" }, "key:b"), " "),
    "key:b gamepad:dpad_down touch:button:channel")
  test.expect_eq(table.concat(menu.rebind({ "key:v", "gamepad:dpad_down" }, "gamepad:x"), " "), "gamepad:x key:v")
  local s = fresh()
  local m = menu.new(s, {})
  menu.toggle(m)
  m.page, m.at = "controls", 1
  menu.tick(m, states({ confirm = true }), 0.1)
  test.expect_eq(m.capture, "steer")
  test.expect(menu.lines(m)[2].raw:find("press a key or button", 1, true))
  menu.tick(m, states({ quit = true }), 0.1)
  test.expect(m.capture == nil, "Escape leaves the binding as it was")
  fresh()
end)
