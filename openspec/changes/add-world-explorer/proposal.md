# Proposal: A world explorer for testing

Milestone: testing tools (after the UI redesign). Asked for to test the generated worlds.

## Why

A generated world is far bigger than one flight shows: a whole planet, six peoples, hundreds of
settlements, realms, dynasties, figures, faiths, wars and holy sites, and the lore written from them.
To test world generation and the lore, a tester has to see all of it at once and follow how it hangs
together, without flying for hours or reading a save file.

## What Changes

- A **world explorer**, opened with F4 in flight (or "World explorer (testing)" on the book's Menu
  page) and closed with F4 or Escape. Alone, the world stands still while it is open, as with the
  menu.
- **The globe** (left): the played planet by biome and height, at a finer resolution than the
  world-creation globe.
  - Drag to turn it; the mouse wheel, R/F or PageUp/PageDown to zoom from the whole planet down to
    a region; A/D or left/right to spin it; + and - chips for touch.
  - Marked on it: every settlement (capitals larger), the holy and other sites, and the carpet.
  - Overlays, turned with Q/E (shoulder buttons): biomes only, realms, cultures, faiths (the land
    coloured by whose it is).
  - Clicking a settlement opens it in the lore explorer.
- **The lore explorer** (right): the history database.
  - An overview (the world's seed, year, variant, eras and how many of each thing there are), then
    a list per kind (cultures, languages, faiths, realms, dynasties, settlements, figures,
    artifacts, sites, events), sorted by name (events by year).
  - An entity's page: its fields, every id a link to follow; what refers to it ("settlements of
    this realm"); the events it took part in or that happened there; and the lore written from it
    (a realm's book of kings, a war told from both sides, a site's inscription, an event as a
    legend told far away).
  - Up/down (W/S, the stick) moves through the rows, Enter opens, Escape goes back; the mouse
    clicks rows and its wheel scrolls.
  - The globe swings to whatever has a place and marks it.
- It is a testing tool: its own words are not translated, and it shows everything, whatever the
  heir has learnt.

## Platforms

Desktop, browser and touch (drag, tap, the zoom chips). Not in VR.

## Co-op

Each heir opens their own; in company the world goes on behind it.

## Impact

- `src/script/script_create.c`: the played world's globe at 96 cells per face edge (the creation
  time-lapse keeps 24).
- New `ui/explorer.lua`; `main.lua` (opening, ticking, drawing), `ui/menu.lua` (the row), `data/input.lua`
  (the explorer action), the row's German.
