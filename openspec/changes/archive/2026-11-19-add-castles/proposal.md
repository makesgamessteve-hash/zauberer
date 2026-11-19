# Proposal: Citadels and castles

## Why

Citadels are the showpiece of procedural architecture (concept 6.6) and the exit demo explores
one of 100+ rooms.

## What Changes

- `places/castles.lua`: siting on the highest ground, curtain wall along the defensible contour
  with wall-walks and crenels, towers, gatehouse with portcullis and murder holes, moat on level
  ground, baileys, palace (audience hall with iwan, quarters, library, shrine), barracks,
  stables, armoury, kitchens, well house, bathhouse, cellars, dungeon, secret passage; rooms up
  to the size asked; states (held, abandoned, ruined, bandits, rival mage, the Ashen); styles by
  family. Castle building types in `data/buildings.lua`.
- M8. No platform differences: all of it is Lua on top of the engine's world queries, the same on desktop, web and WebXR.

## Impact

- New capability `castles`. Co-op: a castle is the same for every heir; garrisons follow its state.
