-- SPDX-License-Identifier: MIT
-- What the game sounds like: the music follows the situation (danger near,
-- then calm again) crossfading over two seconds, the ambience follows the
-- place; actions have sounds (a hit, a defeat, a pickup, a turn of the
-- story) and hits rumble a gamepad unless vibration is off.
-- spec: game-audio: Danger near
-- spec: game-audio: Bitten with a gamepad
scenario { name = "soundscape" }
local game = demo.game
local soundscape = require("player.soundscape")
local settings = require("ui.settings")
local s = game.sound
step(40)
-- The situation and the place, as the rules choose them.
expect_eq(soundscape.situation({ title = true, danger = true }), "title")
expect_eq(soundscape.situation({ story = true, danger = true }), "story")
expect_eq(soundscape.situation({ danger = true, night = true }), "danger")
expect_eq(soundscape.situation({ night = true }), "night")
expect_eq(soundscape.situation({}), "calm")
expect_eq(soundscape.place({ underground = true, rain = true }), "cave")
expect_eq(soundscape.place({ rain = true, aloft = true }), "rain")
expect_eq(soundscape.place({ aloft = true, settlement = true }), "wind")
expect_eq(soundscape.place({ settlement = true }), "village")
expect_eq(soundscape.place({ night = true }), "night")
-- In flight by day: calm music, the wind aloft, both playing.
expect(s.music.name == "calm" or s.music.name == "night", tostring(s.music.name))
expect_eq(audio.state(s.music.voice), "playing")
expect_eq(audio.state(game.music), "playing", "game.music is the music playing")
-- A hunter near: the danger music fades in over two seconds, the calm fades out.
local calm = s.music.voice
local c = game.carpet
c.speed = 0
c.pos.alt = world.ground_height(c.pos) + 2
local wolf = sim.spawn_creature("wolf", world.move(c.pos, 0, 20, 0, 0))
step(40)
expect_eq(s.music.name, "danger")
expect(s.music.vol < soundscape.MUSIC_VOLUME and s.music.old ~= nil, "fading")
step(60 * 2 + 10)
expect(math.abs(s.music.vol - soundscape.MUSIC_VOLUME) < 1e-6 and s.music.old == nil, "faded in")
expect_eq(audio.state(calm), "stopped", "the calm music faded out")
sim.remove_creature(wolf)
step(40)
expect(s.music.name ~= "danger", "calm again: " .. tostring(s.music.name))
-- Sounds of actions; a hit rumbles a gamepad in proportion.
local health = demo.health
local played = #s.played
health.hurt(game.health, "impact", 12)
step(2)
local function heard(name)
  for i = played + 1, #s.played do if s.played[i] == name then return true end end
  return false
end
expect(heard("hit"), "a hit is heard")
expect(#s.rumbles > 0 and math.abs(s.rumbles[#s.rumbles] - 0.4) < 0.01, "and rumbles at 12/30")
demo.pack.say(game.pack, "Taken coin 5")
expect(heard("pickup"), "a pickup is heard")
health.hurt(game.health, "magic", 1000)
step(2)
expect(heard("defeat"), "a defeat is heard")
step(60 * 4)
-- Vibration off: the hit is heard but nothing rumbles.
settings.set(game.settings, "vibration", false)
local rumbles = #s.rumbles
played = #s.played
health.hurt(game.health, "impact", 12)
step(2)
expect(heard("hit") and #s.rumbles == rumbles, "heard, not rumbled")
expect_eq(soundscape.rumble(s, 0.5, 0.2, false), 0)
expect_eq(soundscape.rumble(s, 2, 5, true), 0, "(no gamepad connected)")
expect(not pcall(input.rumble, 2, 1) and not pcall(input.rumble, 0.5, 3), "bad strengths and times are refused")
settings.set(game.settings, "vibration", true)
-- A turn of the story is heard.
played = #s.played
input.inject("story", 1, 1)
step(3)
game.story.chores = { water = true, goats = true, pomegranates = true }
game.story.talked = true
step(40)
expect(heard("beat"), "a turn of the story is heard")
expect_eq(s.music.name, "story", "and the story's music plays for Azhdar's arrival")
