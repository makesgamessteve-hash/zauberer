# Proposal: Mana motes

## Why

The mage's reservoir only empties so far. Concept 3.2 refills it two ways: motes, glowing
specks of one colour's mana that magical beings release when they die, and channeling
from the field while holding still. Vertical Slice A asks the player to collect the
motes of a slain storm bird.

## What Changes

- **Motes** (`magic/motes.lua`):
  - released as a few specks of one colour around a place, each holding a share;
  - they drift with the local wind and bob;
  - they dissolve back into the mana field over two minutes, so no mana is lost;
  - a mage within 15 m draws them in, and within 4 m collects what fits;
  - at most 512 at a time.
- **Blooms**: where a colour runs very rich (near its capacity and above 60), now and
  then some of it gathers into motes taken from the field.
- **Channeling**: a mage holding still (slower than 5 m/s) draws mana from the field at
  the channeling rate, each colour by its share there, up to the capacity.
- **In play**:
  - magical creatures release their motes where they die;
  - the carpet collects motes in flight;
  - the channel action (V, the gamepad's d-pad down, a touch button) channels while
    held;
  - motes are drawn as glowing sprites;
  - the HUD tells what was collected and when channeling.

## Impact

- New capability `mana-motes`.
- Enchanted objects arrive later; they will release motes through the same function
  when destroyed.
