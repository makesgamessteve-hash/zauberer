# Proposal: Several active areas

## Why

Heirs who split up each need their own active simulation area within the host's budget
(concept 11.6, implementation plan 1.11).

## What Changes

- `zb_tier_set_radius` (and the systems' radius setters), `sim.set_active_radius`.
- `coop/areas.lua`: heirs within 2 km share an area, up to four; each area's radius shrinks with
  their number and when the tier runs over its budget, never below 80 m; `coop/play.lua` sets a
  focus per area on the host.
- Platform: none. M9.

## Impact

- New capability `multi-area-simulation`. Co-op: the host runs the areas; clients see results.
