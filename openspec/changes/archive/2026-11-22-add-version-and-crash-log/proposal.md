# Proposal: The version shown, and a crash log

Milestone: M11 (polish after the 1.0 RC).

## Why

Bug reports need the version, and it was shown nowhere in the game. A crash left nothing behind
on the desktop, and in the browser the page simply stopped.

## What Changes

- The version is shown on the title (from add-title-and-game-flow) and in the menu.
- A player's game keeps the last 200 lines logged. On a fatal error (a failed assertion, a fatal
  signal) it writes them with the error to `crash.log` in the user folder and says where.
- In the browser a stopped engine says so on the page, with the last warning or error, that the
  game was saved, and a reload button.
- (Also: the volume settings start as the user configuration has them until the player changes
  them.)

## Platforms

Desktop: assertions and fatal signals. Web: the engine's abort; the page shows the message.
Tests, scenarios and replays keep their own handling.

## Impact

Capability `platform-targets` (added "Version and crash log"). `src/core/crash.{c,h}`,
`src/app/engine.c`, `web/zauberer-loader.js`, `ui/menu.lua`, `ui/settings.lua`.
