# Proposal: The menu pauses play alone; losing focus opens it

Milestone: M11 (polish after the 1.0 RC playtests).

## Why

In single-player the world went on while the menu was open: a wolf could bring the heir down
while the player read the settings. A hidden browser tab, a minimised window or a locked phone
kept the simulation running and the sound playing; a closed tab lost the game since the last save.

## What Changes

- The engine can hold the world still (no game time, no fire, water, air or creatures) while
  input and scripts go on (`engine.hold_world`).
- Alone, the open menu holds the world; in a co-op session the world goes on.
- The window losing focus, being hidden or minimised, the app going to the background, or the
  web page being hidden opens the menu in play and mutes the sound; on the web the game is saved.
  Focus back, the sound returns and the menu stays open.
- `audio.group_volume("master")` reads the master volume back.

## Platforms

Desktop: SDL window focus, hide and minimise events. Web: the page's visibility (another tab, a
locked phone) through the loader. Mobile web: as the web. WebXR: the session's page is visible
throughout; no difference.

## Impact

Capabilities `accessibility` (modified "Escape opens the menu in play", added "Losing focus") and
`game-loop` (added "Holding the world"). `src/core/event.h`, `src/platform/platform_sdl.c`,
`src/app/engine.c`, `src/script/script_api.c`, `src/audio/audio.c`, `web/zauberer-loader.js`,
`assets/base/scripts/main.lua`.
