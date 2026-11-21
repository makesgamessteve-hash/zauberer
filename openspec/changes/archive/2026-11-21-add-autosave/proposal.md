# Proposal: Autosave

Milestone: M11 (polish after the 1.0 RC).

## Why

The game saved only on "save and quit" and every five minutes of flight. A closed browser tab,
a crash or a flat battery lost every turn of the story since then, and the player never knew
whether the game had been saved.

## What Changes

- The game saves on its own every five minutes of play, at each turn of the story (prologue
  beats and saga steps), before sleep, and when the web page is hidden.
- Not while the heir lies defeated: the save is made once they are up again.
- In company only the host saves; a client leaves it to the host.
- A "Game saved" line shows on the HUD for two seconds after each autosave.
- The start says which game it continues.

## Platforms

All the same. The web also saves when the page is hidden (from add-pause-on-menu-and-focus-loss).

## Impact

Capability `world-persistence` (added "Autosave"). `main.lua`, the German strings.
