# Proposal: The carpet's condition

## Why

The carpet is cloth (concept 2.1). It can be singed, torn and soaked, and repaired, and it
grows through upgrades woven into it. M6 makes the carpet a thing to look after. M7's
artificing weaves upgrades into its slots.

## What Changes

- **Condition** (`flight/condition.lua`): integrity (0 .. 100) and wetness (0 .. 1).
  - **Burns:** 8 a second when flying low (within 4 m) over flames.
  - **Tears:** 4 a second when skidding along the ground faster than 30 m/s.
  - **Soaking:** rain soaks it by 0.08 an hour per mm/h, unless weatherproof or not out in
    the open. Standing water soaks it at once.
  - **Drying:** about 2 hours from soaked in mild air, faster when warm, four times faster
    by a fire.
- **In flight** (`carpet.limits`):
  - Wet, it is up to 30 % slower and climbs up to half as fast.
  - Below 30 % it is badly torn and slower still.
  - At zero it cannot fly. It sinks gently and creeps along, until it is patched to 10
    or more.
- **Patching:** a field patch on foot by the carpet (M) mends 20, up to 70. It takes
  2 thread (fibres give 1, a hide 4) and 2 life or void mana. A full repair is left to
  the workshop (M7).
- **Upgrades** (`data/carpet_upgrades.lua`): woven into 3 slots.
  - Weatherproof weave: rain does not soak it.
  - Fire ward lining: a burn does 30 %.
  - Swift weave: 15 % faster.
  - Silent glide: it is half as visible in flight.
- **In the game:** the HUD shows the condition when it is not perfect, and the condition
  is saved with the player.

## Impact

- New capability `carpet-condition`; `carpet-flight` follows the limits.
- M7's artificing weaves the upgrades and its workshop repairs fully.
