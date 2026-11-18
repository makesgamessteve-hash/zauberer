# Design: Weather

## Context

The world has a climate per place (annual mean, coldest and warmest month, rainfall), a
calendar (season, daylight) and noise functions. The active tier's simulations take a
wind and a rain rate. Daylight is computed per frame from the sky.

## Goals / Non-Goals

**Goals:**

- Weather that differs by climate and moves.
- Weather that agrees for every peer and save without being stored.
- Lightning and rain that drive fire, and a visible sky.

**Non-Goals:**

- A fluid-dynamical global model.
- Fog, hail and sandstorms.
- Snow on the ground.
- Clouds in the sky as objects.
- Weather effects beyond fire, water and air.

## Decisions

### A function, not a simulation

Storing and simulating pressure systems over the macro grid would need saving, catch-up
and syncing to peers. Instead, weather is a 4-octave noise field on the sphere:

- its longitude is shifted by the prevailing wind times the time, so systems drift east
  in the westerlies and west in the trades;
- its third coordinate advances with time, so systems change shape over about three
  days.

The place's climate sets the cloud threshold: 0.35 − 0.6 × (rainfall / 2,500 mm),
clamped. Rain falls under cloud above 0.6, at a rate that scales with the climate's
rainfall. It is snow below 0.5 °C of the place's temperature (season, time of day,
height). Storms need cloud above 0.8 and more than 12 °C, gated by a finer noise so they
come in patches. Everything goes through `core/dmath` and the noise functions, so it is
deterministic across platforms.

### One weather in use

`zb_weather_control` holds the weather in use and whether the scripts set it. The engine
recomputes it at the player once a second unless overridden. `sim/tier` applies it to
the simulations:

- the air's wind;
- the water's rain;
- the fire's wind and wetting, where snow counts as water.

Fire takes its lean from the air's local wind, as before.

### Lightning

Each second of a storm rolls `hash(seed, second) < 0.03 × storm`, so a full storm strikes
about every half minute. The strike goes to the highest of six places 40–295 m from the
player, ignites 3 m around it and flashes the scene for four frames. Because the roll is
hashed, replays see the same strikes.

### The look

`zb_daylight_overcast` turns cloud into:

- the zenith colour pulled fully to grey at full cloud, and the horizon 80 % of the way;
- no sun disc;
- direct light reduced by 70 %;
- ambient light reduced by 15 %.

At zero cloud it changes nothing, so every golden image stays. Rain and snow are up to
400 sprites in a 60 m box around the camera, falling at 9 m/s and 1.2 m/s and leaning
with the wind.

## Risks / Trade-offs

- A pure function cannot remember: there is no rain shadow that follows a storm, and
  local fire-driven convection cannot make clouds. Simulation tiers and later weather
  work can add stateful effects on top.
- Rainfall totals follow the climate only roughly. Tune against the climate if needed.
