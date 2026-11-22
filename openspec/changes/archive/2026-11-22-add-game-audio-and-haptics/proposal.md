# Proposal: The sound of the world, and feedback for actions

Milestone: M11 (polish after the 1.0 RC).

## Why

The audio engine was specified, but not what the game sounds like: one music loop played
whatever happened, actions made no sound, and gamepads never rumbled.

## What Changes

- Generated, reproducible audio (tools/gen/gen_sounds.py, CC BY 4.0):
  - music for the title, calm flight, the night, danger and the story;
  - ambiences: wind aloft, a settlement, a cave, rain, the night;
  - effects: casting, a weave going off, a hit, a pickup, a defeat, a turn of the story.
- The music follows the situation (the title and ending, a moment of the story, a hunter near or
  low health, night, else calm). The ambience follows the place (underground, rain, aloft, in a
  settlement, night). Both crossfade over two seconds.
- Actions are heard. Hits rumble a gamepad in proportion to the harm, misfires and defeats too
  (`input.rumble`), unless the new Vibration setting is off.
- A defeat is now noticed from the health itself, whatever dealt it: the hunters near spare the
  heir even when hunger or cold brings them down.
- First-time hints show in a player's game (a test asks for them itself).

## Platforms

Gamepad rumble on desktop (SDL) where the pad supports it; none in the browser or on touch.
Audio is the same everywhere.

## Impact

New capability `game-audio`. `tools/gen/gen_sounds.py`, `assets/base/{music,ambience,sounds}`,
the credits, `player/soundscape.lua`, `main.lua`, `ui/settings.lua`, `ui/menu.lua`,
`src/platform/*`, `src/app/engine.c`, `src/script/script_api.c`.
