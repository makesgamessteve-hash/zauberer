# Proposal: Settlements: siting, layout and land use

## Why

The places the history placed must become streets, plots and buildings when the heirs come
near (concept 6.2-6.4, 6.8), lazily and deterministically.

## What Changes

- `places/layout.lua`: main streets towards road links, ring streets, plots facing them, roles
  near the centre (temple, bazaar, caravanserai, bathhouse, teahouse; wells on the square; smithy
  at the edge; docks on shores), walls with gates and towers, land use (fields by climate,
  pasture, orchards, woodlots, fishing grounds, quarries), terrain fit (plinths, terraces,
  stilts, steps), special plans (caravanserai, nomad camp, mage tower, cliff dwellings,
  citadel), ruins.
- `places/places.lua`, `places/play.lua`, `places/view.lua`: places near any heir laid out within
  3 km, planned within 120 m, furnished within 35 m, forgotten beyond 5 km (changes kept as
  deltas with the save); drawn as shells far and kit pieces near.
- The exit demo `tests/scenarios/places_and_people.lua`.
- M8. No platform differences: all of it is Lua on top of the engine's world queries, the same on desktop, web and WebXR.

## Impact

- New capability `settlements`. Co-op: places come to be around every heir present; the host's
  deltas (doors, taken things, burnt pieces) are the truth.
