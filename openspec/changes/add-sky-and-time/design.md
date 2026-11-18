# Design: Sky and Time

## Context

The renderer has an atmosphere (zenith and horizon colours, a sun disc and fog) and one
directional light. Scripts have set both statically. Created worlds record an axial
tilt. Plants already carry per-season colours.

## Goals / Non-Goals

**Goals:**

- A day, a night and a year that read clearly from the carpet.
- Deterministic.
- Cheap.
- Seasons that show in plants and snow.

**Non-Goals:**

- Weather and clouds (`add-weather`, M3).
- Light from fires and spells.
- Shadows.
- Tides.
- Sleeping and time skips in co-op (M9).
- Eclipses.

## Decisions

### Time is game seconds; everything else is a function of it

The world holds one number, game seconds since its start. The date, the sun, the moon
and the daylight are pure functions of it and the calendar settings (day length,
axial tilt). The engine advances it each tick by the tick's real seconds converted to
game seconds. Nothing in the simulation depends on it yet, so replay hashes do not
change.

The sun follows the planet's spin (one solar day per 86,400 game seconds; the year's
orbital motion is folded in). Its declination is asin(sin ε · sin(2π·year fraction)),
with year fraction 0 at the spring equinox.

The moon has a synodic period of 12 days. It stands at the sun's longitude plus
2π·phase, with a matching declination, so it is new beside the sun and full opposite
it. Its lit fraction is (1 − cos 2π·phase) / 2.

### Daylight as data for the renderer

`zb_daylight_compute` turns the sky above the camera into an atmosphere, a light and
an ambient term:

- Colours blend night, day and twilight. Twilight is the sun within 0.3 rad of the
  horizon, which turns the horizon orange.
- By night the light comes from the moon, scaled by its lit fraction and height.
- The engine applies this each frame only when the scripts ask for it with
  `render.daylight(true)`. Scenes with their own fixed light (the showcase, the
  new-world globe) keep it.

### Stars in the sky shader

The sky shader maps the view ray into a star frame: the local axes rotated by the
planet's spin. A star is a cell of a 180-per-unit grid whose hash passes a threshold.
Stars fade in with the night and out near the horizon, and turn once per day. The
moon's disc and halo use its direction and lit fraction. No textures and no extra
draw calls.

### Seasons per view and per chunk

- Plants use the season at the camera, shifted by half a year south of the equator.
  It is one season for all chunks in view, which is fine within the view distance.
- Snow is per chunk: the season's temperature from the chunk's climate (annual mean
  plus half the yearly range on a sine through the year, minus the lapse rate at the
  chunk's mean height) gives a snow amount. The terrain shader blends flat ground
  towards white by it.
- The chunk climate is cached with its mesh, so this costs a sine per chunk per frame.

## Risks / Trade-offs

- The moon has no inclination, so eclipses would happen every month. They are not
  drawn: the sun's disc is drawn over the moon's.
- Snow is coarse: one amount per chunk, with no snowline inside a chunk. The material
  rule's permanent snow on high peaks remains.
