# Proposal: Buildings: program, massing, floor plans, kit pieces, furnishing, set pieces

## Why

Every house can be entered (concept 6.5): buildings are generated in layers, deterministic
from their seed, assembled from kit pieces on a 0.5 m grid; story places come from templates
(6.9) and Vertical Slice B needs them.

## What Changes

- `data/buildings.lua`, `places/buildings.lua`: programs by type and household; massing by style
  family and land (roofs, walls, courtyards, iwans, windcatchers); floor plans (courtyard rings,
  corridors, guillotine subdivision), doors from the entrance, windows, stairs; kit pieces
  (segments, lintels, floors, roofs, plinth, stilts) each of a material; furnishing (against
  walls with clearance, groups, lights, containers by household); contents when opened.
- `tools/gen/gen_kit.py`: 47 kit models (box, dome, gable, vault, cone, cylinder by material).
- Seamless interiors: the walker stands on floors and stairs, walls and closed doors stop it;
  "use" (Tab) opens doors and containers.
- Set pieces (`places/setpieces.lua`): Anarvan's template; the prologue (`story/prologue.lua`,
  `story/director.lua`, F5) is Vertical Slice B.
- M8. No platform differences: all of it is Lua on top of the engine's world queries, the same on desktop, web and WebXR.

## Impact

- New capability `building-generation`; `vertical-slice` gains Vertical Slice B. New items
  (trade goods, coins, books), the `use` and `story` actions; the demo replay is re-recorded.
