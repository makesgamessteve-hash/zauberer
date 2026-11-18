# Proposal: The M5 exit demo, Living Things

## Why

M5's exit demo asks for three things:
- survive seven in-game days in two contrasting biomes by foraging, hunting and cooking;
- get sick from swamp water and cure it with boiled water and herbs;
- watch predators follow scent downwind.

Playing it through in a created world brought up gaps that are fixed alongside it:
- wet lands were bare;
- forests had no firewood;
- gathering at a grassy bank took the grass, not the water;
- mosquitoes gave fever before the swamp water could;
- survival and pack news vanished in a tenth of a second.

## What Changes

- **The demo** (`tests/scenarios/living_things.lua`), in a created world (seed 11), on one
  river with two camps: a tropical cloud forest and the reed estuary downstream.
  - The camps are found by the river and the biomes beside it.
  - Over seven days, in a fair week, the mage:
    - hunts a boar with Fireball and butchers it;
    - forages coconuts, reed roots, firewood and kapok bark;
    - lays campfires and lights them with Kindle;
    - cooks and boils water;
    - eats and drinks as hunger and thirst come;
    - sleeps by the fire.
  - The days pass in a time-lapse of four game minutes a tick. All of it is lived: needs,
    warmth and spoiling count every minute.
  - One evening a breeze along the bank carries the mage's scent. A hungry wolf downwind
    tracks it and comes upwind; one as far upwind never comes.
  - In the estuary, with no fire yet, the mage drinks from the river unboiled and falls
    sick from the swamp water. A fire of reeds, boiled water and bark tea cure it.
  - After seven days the mage is alive, fed, watered and well.
- **Fixed with it** (found on the way):
  - `sim.set_weather` takes the air's temperature, which it silently ignored, so held
    weather froze whatever the air was at that moment (0 C before the weather had
    warmed up);
  - clothes and a fire warm the body only as far as it wants (up to a felt 25 C): in
    the heat clothes come off and a fire is sat back from, so a camp in the tropics no
    longer overheats.
- **Fixes found on the way** (committed before this change):
  - wet-tolerant plants (reeds, fern, kapok, the coconut palm) bear the 4000 to 5500 mm
    of rain of created worlds' wettest lands;
  - every tree drops dead branches, deadwood to take by hand;
  - kapok bark steeps into bark tea, and reed fibre burns;
  - gathering takes the water the mage faces first;
  - the smoke of a fire close by keeps mosquitoes off;
  - survival and pack news fade in real seconds;
  - world.plants skips chunks out of reach (ten times faster);
  - biome names match the habitat tables.

## Impact

- `survival` gains the M5 exit demo, the smoke rule and adaptive warmth.
- `weather`: scripts may set the air's temperature.
- `harvest-and-cooking`'s gathering faces the water.
- The demo runs about 9000 ticks in the headless suite.
