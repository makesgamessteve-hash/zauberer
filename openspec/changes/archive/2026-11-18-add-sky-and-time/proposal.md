# Proposal: Sky and Time

## Why

Concept §4.2 gives the planet:

- a day of 40 real minutes;
- a year of 4 seasons × 12 days;
- an axial tilt that makes seasons and polar day and night;
- a moon for night light and rituals.

§7 says time passes. M2's planet so far has a fixed noon sun and an eternal summer.
This change gives it days, nights and seasons that the player sees.

## What Changes

- **World time and calendar** (`world/calendar`):
  - Game seconds since the world began, advanced every tick at 36 game seconds per
    real second (a 40-minute day by default).
  - The date: year, season, day and clock.
  - The sun's direction from the planet's spin and the axial tilt, which comes from the
    created world's option (23° by default). This gives noon overhead in the tropics,
    polar day and night, and opposite seasons in the south.
  - A moon on a 12-day cycle with its phase and lit fraction, new beside the sun and
    full opposite it.
- **Daylight** (`app/daylight`): from the sky above the camera, the renderer gets:
  - sky colours (day blue, orange twilight, night blue that a moon brightens);
  - the sun's colour and disc;
  - a light that is the sun by day and the moon by night;
  - an ambient term;
  - stars, which turn with the sky as the planet spins;
  - the moon's disc, lit by its phase.
- **Seasons in the landscape**:
  - Plants take the colours of the local season (`add-vegetation` tints), half a year
    apart in the south.
  - Snow covers flat ground where the season's temperature at that height is below
    freezing.
- **Lua**:
  - `world.time()` gives the date.
  - `world.set_time(seconds)` sets it, for sleeping, travel and tests.
  - `world.sky(pos)` gives the sun and moon elevation, the moon phase, the daylight
    and the season.
  - `render.daylight(on)` lets the engine light the scene from the sky. Flight turns it
    on; the showcase and new-world screens keep their fixed light.
  - The flight HUD shows the day, season and time.

## Capabilities

### New Capabilities

- `sky-and-time`: world time and calendar, the sun and moon, daylight, stars and moon
  in the sky, and seasons in the landscape.

### Modified Capabilities

None. The sky shader gains stars and a moon; the sky colour reference
(`zb_sky_color`) is unchanged for skies without them.

## Impact

- New:
  - `src/world/calendar.{h,c}` and `src/app/daylight.{h,c}`;
  - tests `test_calendar` and `test_daylight`;
  - scenarios `day_and_night` (headless) and `night_sky` (rendered).
- Changed:
  - the world (time and calendar), the engine (advancing time, applying daylight);
  - the renderer: atmosphere fields for night, moon and stars, and snow on terrain
    draws;
  - the GL sky and terrain shaders;
  - the terrain view (season and snow per chunk);
  - Lua `world` and `render`, and the flight HUD.
- Platforms: all of it is a little arithmetic per frame. The star field is a hash in the
  sky shader, which ES 3.00 handles.
- Co-op: time is part of the world state that the host owns and sends to clients
  (`add-coop-sessions`, M9). The sky is a pure function of the time.
