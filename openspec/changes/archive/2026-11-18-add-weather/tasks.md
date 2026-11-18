# Tasks

## 1. World weather

- [x] 1.1 `world/weather`: prevailing winds, drifting systems, climate-driven cloud and rain, snow, storms, gusts; verify "weather: Prevailing winds by latitude", "Climate decides the weather", "Weather systems drift with the wind" and "Snow, rain and thunderstorms"

## 2. In the game

- [x] 2.1 `sim/tier` applies a weather to fire, water and air; the engine applies the world's weather each second unless scripts set one
- [x] 2.2 Lightning in storms, `sim.lightning`; overcast daylight and the flash; verify "Clouds darken the sky"
- [x] 2.3 Rain and snow sprites (generated art) drawn around the camera
- [x] 2.4 Lua `world.weather`, `sim.weather`, `sim.set_weather` (cloud, snow, storm), `sim.natural_weather`; scenarios; verify "Weather in the game"
- [x] 2.5 Tag every scenario, regenerate the Lua API reference, keep coverage ≥ 95 %
