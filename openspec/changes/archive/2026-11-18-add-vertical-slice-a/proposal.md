# Proposal: Vertical Slice A

## Why

M4 ends in a playable slice (implementation plan, M4): a short demo where the first
magic, the first creatures and the simulation meet. It proves the pieces work together.
The player flies out, scares wolves with fire, sets the grass alight by accident, and
puts it out with Unmake Fire. A storm bird is brought down and its motes collected, and
water is frozen to walk out over it.

## What Changes

- **The slice played through** (scenario `slice_a.lua`) with the base game's own
  creatures, weaves, hands and bindings:
  - Kindle at the grass in front of a wolf pack, which flees;
  - the fire spreads and Unmake Fire (with two wide modifiers) puts it out;
  - a Fireball, led like a player would lead a moving target, brings the storm bird
    down, and the carpet collects its storm motes;
  - at the water the mage steps off and wades out until it is too deep, freezes it
    with Freeze and walks out onto the ice;
  - back on the carpet, and away.
- **The first world has the sea but no lakes** (lakes come with created worlds, and
  are rare near a start), so the slice freezes the sea off the nearest shore; in a
  created world the same works on a lake.
- **Fixes the playthrough needed**:
  - a dead flier falls to the ground (the storm bird used to hang in the air);
  - a bolt strikes a creature within 2 m (it was 1.5 m, too fine to hit a bird in
    flight).

## Impact

- New capability `vertical-slice`: the slices the milestones end with.
- `creatures` and `damage-model` change as above.
- The milestone gates (coverage, sanitizers, web) run after it.
