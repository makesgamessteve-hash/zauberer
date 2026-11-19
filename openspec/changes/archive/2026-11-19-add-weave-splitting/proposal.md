# Proposal: One carpet, several heirs

## Why

There is one Great Weave (concept 11.2): the heirs ride it together or split it into a carpet
each, and the tent is pitched from the whole.

## What Changes

- `coop/weave.lua`: riding (one pilot, the others cast), splitting into pieces weaker by their
  share and bond, rejoining when close, damage carried over, upgrades in every piece, Homing.
- `flight/coop.lua`: riders sit on the pilot's carpet and are not predicted; `coop/play.lua`:
  F6 splits or rejoins; the tent needs the whole Weave in co-op.
- Platform: none. M9.

## Impact

- New capability `weave-splitting`. Co-op: the host decides the Weave's state and carries the riders.
