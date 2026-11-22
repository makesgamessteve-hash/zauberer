# Proposal: A map of the known world

Milestone: M11 (polish after the 1.0 RC).

## Why

The planet is endless, and settlements, ruins and story sites were given in words only
("a day's ride to the east"). Players got lost, and the bots did too.

## What Changes

- A Map page in the menu shows:
  - the land the heir has seen (cells of a kilometre within two of where they went), by height
    as sea, lowland, highland and peaks; unseen land stays blank;
  - the places the heir came near, by name;
  - the heir (@), the carpet when on foot (C) and the current goal (X).
- Positions are measured from where the carpet first set out (distance and bearing). The stick
  moves the map and confirm zooms out, through 1, 2, 5, 10 and 20 km a letter.
- The map is kept with the save.

## Platforms

All the same: the stick, arrow keys or the touch stick move the map; OK or Enter zooms.

## Co-op

Each heir keeps their own map (the shared map is left for later).

## Impact

New capability `world-map`. `ui/worldmap.lua`, `ui/menu.lua`, `main.lua`, the German strings.
