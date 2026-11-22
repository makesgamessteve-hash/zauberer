-- SPDX-License-Identifier: MIT
-- The player's settings for sound, the picture and the controls: volumes
-- (master, music, sounds) and mute heard at once, V-sync and a frame cap,
-- the pixel resolution and brightness, invert look; larger text wraps to
-- the screen; all kept, invalid values refused, and back to the defaults
-- (keeping the language) after the question is answered yes.
-- spec: settings: Changing and keeping settings
-- spec: settings: Large text
-- spec: settings: Reset
scenario { name = "player settings" }
local game = demo.game
local settings = require("ui.settings")
local text = require("ui.text")
local menu = require("ui.menu")
local s = game.settings
step(5)
local function near(a, b) return math.abs(a - b) < 1e-6 end
-- Sound: the music silent, the sounds untouched; mute and back.
expect(settings.set(s, "music_volume", 0))
expect(near(audio.group_volume("music"), 0) and near(audio.group_volume("effects"), 1), "music silent, sounds on")
expect(settings.set(s, "master_volume", 60))
expect(near(audio.group_volume("master"), 0.6))
expect(settings.set(s, "mute", true))
expect(near(audio.group_volume("master"), 0), "muted")
expect(settings.set(s, "mute", false))
expect(near(audio.group_volume("master"), 0.6), "heard again")
-- The picture and its pace.
expect(settings.set(s, "pixel_height", 480))
expect_eq(render.internal_height(), 480)
expect(settings.set(s, "brightness", 1.25))
expect(near(render.brightness(), 1.25))
expect(settings.set(s, "vsync", false) and settings.set(s, "frame_cap", 60))
expect(not settings.set(s, "frame_cap", 45) and not settings.set(s, "brightness", 3), "invalid values refused")
expect(not pcall(engine.set_frame_cap, 7), "a frame cap out of range is refused")
-- Invert look: looking up turns the view down.
input.inject("look", 0, 1, 3)
step(1)
local plain = input.state("look").y
expect(settings.set(s, "invert_look", true))
input.inject("look", 0, 1, 3)
step(1)
expect(plain > 0 and input.state("look").y < 0, "looking up inverts: " .. plain .. " / " .. input.state("look").y)
-- Kept for the next start.
local again = settings.load()
expect(again.music_volume == 0 and again.invert_look and again.pixel_height == 480 and again.brightness == 1.25)
-- Large text wraps inside the screen.
expect(settings.set(s, "text_size", 2))
local long = string.rep("a long line of help ", 20)
local pieces = text.wrap(long, 40)
expect(#pieces > 1, "wrapped")
for _, p in ipairs(pieces) do expect(utf8.len(p) <= 40, p) end
expect_eq(#text.wrap("short", 40), 1)
expect_eq(#text.wrap(string.rep("x", 100), 40), 3, "a word longer than the line is cut")
text.lines(demo.assets.font, { long })
-- Back to the defaults, after the question; the language stays.
settings.set(s, "language", "de")
input.inject("quit", 1, 1)
step(2)
game.menu.page = "reset"
game.menu.at = 1
input.inject("confirm", 1, 1) -- No: kept
step(2)
expect_eq(game.menu.page, "settings")
expect_eq(s.music_volume, 0, "kept after No")
game.menu.page, game.menu.at = "reset", 2
input.inject("confirm", 1, 1) -- Yes
step(2)
expect(s.music_volume == 100 and not s.invert_look and s.pixel_height == 360 and s.brightness == 1 and s.text_size == 1,
  "back to the defaults")
expect_eq(s.language, "de", "the language stays")
expect_eq(render.internal_height(), 360)
local lines = {}
for _, l in ipairs(menu.lines(game.menu)) do lines[#lines + 1] = type(l) == "table" and l.raw or l end
settings.set(s, "language", "en")
input.inject("quit", 1, 1)
step(2)
input.inject("quit", 1, 1)
step(2)
expect(not game.menu.open)
