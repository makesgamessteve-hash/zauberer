# Proposal: Rise and fall: the history simulation

## Why

The optional deep variant of peopling (concept 10.6): societies simulated over thousands of
years, written into the same history database.

## What Changes

- `people/simulation.lua`: decades over 4,000 years: logistic growth to capacity, migration,
  chiefdoms, realms growing, wars (numbers, terrain, fortune), sacks, collapse, nomad raids,
  plagues, famine, eruptions, prophets, great works; the present peoples and realms (fallen ones
  kept for history), wars with battlefields and grudges, ruins with causes.
- World creation takes `history = "rise"`; such a world keeps its history in the save
  (`history.save`/`history.load`) and takes it back without simulating again.
- Budget: under 10 s on a desktop for the default planet, in slices.
- M8. No platform differences: all of it is Lua on top of the engine's world queries, the same on desktop, web and WebXR.

## Impact

- New capability `history-simulation`. Co-op: the host's saved history is the world's; clients
  receive it with the world bundle (M9).
