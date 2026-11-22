-- SPDX-License-Identifier: MIT
-- What the game sounds like: music by the situation (the title, calm flight,
-- night, danger near, a moment of the story), an ambience by the place (wind
-- aloft, a settlement, a cave, rain, the night), each crossfading over two
-- seconds; a sound for each of the heir's actions (casting, a weave going
-- off, a hit, a pickup, a defeat, a turn of the story); and a gamepad's
-- rumble for hits and misfires, in proportion to the harm.
local soundscape = {}

soundscape.FADE = 2 -- seconds a crossfade takes
soundscape.MUSIC = { title = "music/title.ogg", calm = "music/loop.ogg", night = "music/night.ogg",
  danger = "music/danger.ogg", story = "music/story.ogg" }
soundscape.AMBIENCE = { wind = "ambience/wind.ogg", village = "ambience/village.ogg", cave = "ambience/cave.ogg",
  rain = "ambience/rain.ogg", night = "ambience/night.ogg" }
soundscape.SOUNDS = { cast = "sounds/cast.wav", misfire = "sounds/misfire.wav", hit = "sounds/hit.wav",
  pickup = "sounds/pickup.wav", defeat = "sounds/defeat.wav", beat = "sounds/beat.wav" }
soundscape.MUSIC_VOLUME, soundscape.AMBIENCE_VOLUME = 0.5, 0.35

function soundscape.new()
  return { music = {}, ambience = {}, played = {}, rumbles = {} }
end

-- The music for a situation. ctx: { title, story, danger, night }.
function soundscape.situation(ctx)
  if ctx.title then return "title" end
  if ctx.story then return "story" end
  if ctx.danger then return "danger" end
  if ctx.night then return "night" end
  return "calm"
end

-- The ambience for a place. ctx: { underground, rain, aloft, settlement, night }.
function soundscape.place(ctx)
  if ctx.underground then return "cave" end
  if ctx.rain then return "rain" end
  if ctx.aloft then return "wind" end
  if ctx.settlement then return "village" end
  if ctx.night then return "night" end
  return "wind"
end

-- One channel (music or ambience) towards a sound: the new one fades in,
-- the old one out, over soundscape.FADE seconds.
local function channel(ch, name, path, top, group, dt)
  if ch.name ~= name then
    if ch.old then audio.stop(ch.old.voice) end
    ch.old = ch.voice and { voice = ch.voice, vol = ch.vol } or nil
    ch.name, ch.voice, ch.vol = name, audio.play(path, { loop = true, group = group, volume = 0 }), 0
  end
  local step = top * dt / soundscape.FADE
  ch.vol = math.min(top, ch.vol + step)
  audio.set_volume(ch.voice, ch.vol)
  if ch.old then
    ch.old.vol = ch.old.vol - step
    if ch.old.vol <= 0 then
      audio.stop(ch.old.voice)
      ch.old = nil
    else
      audio.set_volume(ch.old.voice, ch.old.vol)
    end
  end
end

-- One tick: the music and ambience follow ctx (see situation and place).
function soundscape.tick(s, dt, ctx)
  local m = soundscape.situation(ctx)
  channel(s.music, m, soundscape.MUSIC[m], soundscape.MUSIC_VOLUME, "music", dt)
  local a = soundscape.place(ctx)
  channel(s.ambience, a, soundscape.AMBIENCE[a], soundscape.AMBIENCE_VOLUME, "effects", dt)
end

-- Plays the sound of an action.
function soundscape.play(s, name, volume)
  audio.play(soundscape.SOUNDS[name], { group = "effects", volume = volume or 0.8 })
  s.played[#s.played + 1] = name
  if #s.played > 32 then table.remove(s.played, 1) end
end

-- A gamepad's rumble (not when vibration is off): strength 0 .. 1.
function soundscape.rumble(s, strength, seconds, on)
  if not on then return 0 end
  strength = math.max(0, math.min(1, strength))
  s.rumbles[#s.rumbles + 1] = strength
  if #s.rumbles > 32 then table.remove(s.rumbles, 1) end
  return input.rumble(strength, math.min(2, seconds))
end

return soundscape
