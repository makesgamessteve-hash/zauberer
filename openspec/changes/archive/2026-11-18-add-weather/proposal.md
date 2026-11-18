# Proposal: Weather

## Why

Concept 5.9 wants global weather systems that move with the prevailing winds, bring
cloud, rain and snow as the climate allows, and build thunderstorms whose lightning
starts fires. Until now the wind and rain of the active tier came from scripts. The M3
exit demo needs this: lightning ignites a dry forest, and rain douses it.

## What Changes

- **World weather** (`world/weather`): a pure function of the world, the place and the
  game time, so peers and saves agree without storing it:
  - systems about 1,500 km across drift with the prevailing winds of their latitude
    (easterly trade winds, westerlies, polar easterlies) and change shape over a few
    days;
  - the place's annual rainfall sets how often they bring cloud and rain;
  - the season, the time of day and the height set the temperature, and with it rain
    or snow;
  - warm, heavy cloud builds thunderstorms in patches;
  - storms and cloud strengthen the wind, and gusts vary it by the minute.
- **The weather in use**:
  - once a second the engine takes the world's weather at the player and applies it to
    fire (wind; rain and snow wet the fuel), surface water (rain) and the air (wind);
  - scripts can set a weather instead (`sim.set_weather`, now also cloud, snow and
    storm) until `sim.natural_weather()`.
- **Lightning**: in a storm, strikes come at the highest of a few places near the player,
  about once a minute in a full storm. They set fire to dry fuel and flash the scene.
  `sim.lightning(pos)` strikes on purpose.
- **What the player sees**:
  - clouds grey the sky, hide the sun's disc and soften the light;
  - rain and snow fall around the camera (generated sprites).
- **Lua**: `world.weather(pos)` reads the world's weather anywhere; `sim.weather()`
  reports the weather in use, whether it is natural, and lightning strikes so far.

## Capabilities

### New Capabilities

- `weather`: the world's weather, its use by the active tier, lightning, and its look.

### Modified Capabilities

None. The simulations' weather inputs are unchanged.

## Impact

- New: `src/world/weather.{h,c}`, `src/sim/tier.{h,c}` (applying a weather to the
  active tier), the rain and snow sprites, tests `test_weather`, scenarios `weather`
  and `weather_view`.
- Changed:
  - the engine (weather each second, lightning, overcast daylight, precipitation);
  - `app/daylight` (`zb_daylight_overcast`);
  - Lua `world` and `sim`.
- Determinism: the weather is a pure function of time. Lightning rolls are hashed from
  the world seed and the second, so a replay sees the same strikes.
- Not yet:
  - snow lying on the ground and melting;
  - fog, hail, sandstorms and rainbows;
  - weather effects on flight, creatures, mana and survival (their changes);
  - clouds as sprites in the sky.
