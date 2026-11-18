# Tasks

## 1. Time

- [x] 1.1 `world/calendar`: date, sun, moon; verify "sky-and-time: A calendar of days and seasons", "The sun crosses the sky", "The moon and its phases" and "Deterministic sky"
- [x] 1.2 World time advanced by the engine every tick; tilt from the created world; verify "Time passes in the game"

## 2. Daylight

- [x] 2.1 `app/daylight`: sky colours, light, ambient, stars and moon frame; verify "Day, twilight and night"
- [x] 2.2 Sky shader: stars and moon; engine applies daylight when asked; verify "The night sky is drawn"
- [x] 2.3 Seasons: plant tints from the camera's season, snow per chunk in the terrain shader; verify "Snow in winter"

## 3. Lua

- [x] 3.1 `world.time`, `world.set_time`, `world.sky`, `render.daylight`; flight HUD; verify "Day and night from Lua"
- [x] 3.2 Tag every scenario, regenerate the Lua API reference, keep coverage ≥ 95 %
