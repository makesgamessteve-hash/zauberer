# Proposal: Ecology

## Why

M5 is about living things. Animals should belong to the land, not be placed by hand:
more deer in forests, hares on open land, cranes in the wetlands. Their numbers should
answer hunting, predators and time, and they should be there when the player comes
back. That is the base for foraging and hunting to survive, and for predators that
follow prey.

## What Changes

- **Species for ecology** (`data/creatures.lua`): density where the habitat suits
  best, habitat by land class (forest, open, wetland, dry, cold), growth per game year,
  group sizes, and the food web: what each predator eats and how hard. New species:
  brown hare, red fox, wild boar and common crane.
- **Populations per chunk** (`life/ecology.lua`):
  - a chunk's habitat comes from its biome in a created world, else from its plants,
    ground and water;
  - each species has a count growing logistically towards its capacity there;
  - predators eat the counts of their prey down, and their own capacity follows how
    well they are fed;
  - species flow from chunks where they are crowded to neighbouring ones where they
    are thin;
  - counts catch up lazily, in steps of a day, at most two years at once.
- **Virtualisation:**
  - the chunks around the player spawn individuals from their counts: herds and packs
    as groups, at places that are the same for a chunk, species and day, never in
    deep water;
  - when the player leaves, the individuals fold back into the counts and the dead
    are taken off.
- **In the game:** the populations are kept with the game and stay when flying on in
  the same world. The fixed groups near the start (wolves, deer, the storm bird) stay
  as they were.

## Impact

- New capability `ecology`.
- Hunting between individuals, herding, thirst and scent tracking follow in
  `add-fauna-behaviour`. Succession after fire and felling already comes from the
  regrowth of plants (`world-persistence`, `plant-life-cycle`). Epidemics and
  seasonal migration over long distances come later.
