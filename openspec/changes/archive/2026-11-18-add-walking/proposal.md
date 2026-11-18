# Proposal: Walking

## Why

The carpet is the way to travel, but some places must be reached on foot (concept
8.5): a frozen lake to cross, a meadow to gather in, a cave mouth. Wolves only become a
real threat to a mage on the ground. Vertical Slice A ends with crossing a frozen lake
on foot.

## What Changes

- **On foot** (`player/walker.lua`):
  - stepping off a carpet resting low (within 4 m of the floor) and slow (under 3 m/s),
    beside it; stepping back on within 3 m of it;
  - walking (1.6 m/s) and running (5 m/s), turning with the stick or the mouse,
    jumping, falling off edges and following the ground down slopes;
  - ice is a floor to stand on; open water deeper than 0.9 m stops the mage at the
    shore;
  - a first-person view out of the eyes.
- **In play**:
  - the mount action (E) steps off and on, and jump (Space, gamepad A) jumps;
  - while on foot the creatures know the player is on foot, the weaves leave the hands
    of the walker along the view, motes are collected and the player is harmed and
    healed where the walker is;
  - the HUD says when the carpet is low and slow enough to step off, and when on foot,
    on ice or at water too deep;
  - a save made on foot comes back on foot.

## Impact

- New capability `walking`.
- Climbing, swimming and carrying follow later.
