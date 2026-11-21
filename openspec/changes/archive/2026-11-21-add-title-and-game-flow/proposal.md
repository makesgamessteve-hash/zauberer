# Proposal: A title screen and the game's flow

Milestone: M11 (polish after the 1.0 RC).

## Why

The game started in flight with no title: there was no "continue", no way to play another
saved game or delete one, and no version shown. Players did not know a game was saved at all.

## What Changes

- A player's game (not a test, scenario or replay; `engine.info().play`) begins at the title over
  the world, which waits there: the name and version, then Continue (or New game at the first
  start), Create a world, Load game (when games are saved), Settings, Controls, Credits and Quit
  (not on the web, where the page is closed).
- Load game lists the saved games newest first with their game day. A chosen game can be played
  (the one being played is saved first) or deleted after a question answered yes; the game being
  played cannot be deleted. `world.delete_save` removes a save.
- While the land under the heir is still being made, the HUD says how far it has got.
- The title is the menu's first page: the same keys, gamepad buttons and touch controls work.

## Platforms

All the same, except that the web title has no Quit.

## Impact

New capability `game-flow`. `ui/menu.lua`, `main.lua`, `src/script/script_save.c`,
`src/script/script_api.c`, `src/app/engine.c`, the German strings.
