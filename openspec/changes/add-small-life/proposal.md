# Proposal: Small life

## Why

Survival in M5 needs the life too small to be creatures:
- mushrooms to forage after rain;
- food that spoils;
- water that is dirty in swamps and near carrion;
- carcasses that rot;
- mosquitoes that carry fever over wet ground.

Insects also make the land feel alive: bees at blossoms, fireflies on summer nights.

## What Changes

- **Fungi** (`data/fungi.lua`, CC BY 4.0): chanterelle, porcini, fly agaric, meadow
  mushroom and tinder bracket, each with its habitat (forest, open land, dead trees),
  season and density. Mushrooms fruit in 32 m patches by habitat, season, autumn and
  how wet the ground is. A picked patch fruits again the next day, and picking is kept
  with the game.
- **Microbes as rates** (`life/smalllife.lua`):
  - food spoils logistically, doubling its pace every 10 degrees, faster when wet,
    slower cooked or dried, not at all when frozen;
  - water is salty in the sea, clean in rivers and lakes, dirty in still pools and very
    dirty in swamps, and carrion fouls it;
  - carcasses rot away in three game days.
- **Insects:**
  - swarms of bees by plants in flower (not grass) on bright days;
  - butterflies over sunny meadows in flower;
  - mosquitoes over wet ground at dusk and on warm nights in the growing season;
  - fireflies on summer nights over open ground;
  - each a few particles wandering about an anchor near the player, drawn as small
    generated sprites.
  - Mosquitoes bite near their swarm, and bees pollinate the plants about them.
- **Items:** the five fungi.

## Impact

- New capability `small-life`; the art generators make four insect sprites.
- What sickness and spoilt food do to the mage comes with `add-survival`, picking and
  eating with `add-harvest-and-cooking`. Fermentation, pests and epidemics come later.
