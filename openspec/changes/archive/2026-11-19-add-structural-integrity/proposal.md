# Proposal: Structural integrity

## Why

Kit pieces of materials let fire, force and decay act on buildings (concept 6.5, 6.8): what
loses its support falls.

## What Changes

- `places/integrity.lua`: supports between pieces (lintels from the side), held from the ground,
  loads down the supports, capacity by material, collapse to rubble, overload, removal; fire burns
  timber, thatch, felt and bamboo pieces where the fire simulation burns; ruins settle by it.
- M8. No platform differences: all of it is Lua on top of the engine's world queries, the same on desktop, web and WebXR.

## Impact

- New capability `structural-integrity`. Co-op: the host settles buildings; lost pieces travel
  as deltas.
