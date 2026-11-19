# Proposal: Caves and dungeons

## Why

Under the land lie caves and dungeons (concept 6.7); the exit demo descends into a karst cave
with an underground river.

## What Changes

- `places/caves.lua`: sparse voxel caves from a graph of chambers and passages (a loop, a
  branch), formations, an underground river and lake, its own 3D simulation of water, heat and
  foul air, smooth floors; kinds by land (karst, lava tube, sea cave, glacier cave).
- `places/dungeons.lua`: graph grammar (locks after keys, loops not round locks, branches,
  puzzles), grid layout, kit pieces; puzzles solved through the simulation.
- `places/underground.lua`: mouths in the land, dungeons under tombs, elder ruins, mines,
  sanctums, crypts, catacombs and hideouts; floors under ground, keys, locks, treasure.
- M8. No platform differences: all of it is Lua on top of the engine's world queries, the same on desktop, web and WebXR.

## Impact

- New capability `caves-and-dungeons`. Co-op: caves and dungeons are the same for every heir;
  the host's opened doors and taken things are the truth.
