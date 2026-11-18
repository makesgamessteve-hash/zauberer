# small-life Specification

## Purpose
Defines the life too small to be creatures: fungi in patches, microbes as rates of
spoilage, dirty water and rot, and insects as swarms.

## Requirements

### Requirement: Fungi
Fungi SHALL be data with a habitat (forest, open, dead trees), a season and a density; mushrooms SHALL fruit in 32 m patches by habitat, season, autumn and wet ground, give known items when picked, be picked clean for the day, fruit again the next day, and be kept with the game as picked.

#### Scenario: Fungi
- **WHEN** the fungi data is read
- **THEN** each fungus gives a known item in a season with a habitat and a density

### Requirement: Microbes as rates
Food SHALL spoil logistically from fresh to rotten, doubling its pace every 10 degrees, faster when wet, slower when cooked or dried, not at all when frozen; water SHALL be salty in the sea, clean in rivers and lakes, dirty in still pools, very dirty in swamps, and dirtier near carrion; carcasses SHALL rot away in three game days.

#### Scenario: Microbes as rates
- **WHEN** raw meat waits a warm day, a cold day, frozen, wet, cooked and dried, and for ever in the heat
- **THEN** the warm day spoils it, cold keeps it, frozen it does not change, wet spoils faster and cooked and dried slower, and it never passes rotten

### Requirement: Insects
Swarms SHALL appear near the player by place, season and hour: bees by plants in flower on bright days, butterflies over sunny meadows in flower, mosquitoes over wet ground at dusk and on warm nights in the growing season, fireflies on summer nights over open ground; they SHALL be drawn as small sprites near the ground; mosquitoes SHALL bite near their swarm and bees SHALL pollinate about them.

#### Scenario: Small life in the game
- **WHEN** an autumn day after rain, patches near the carpet are searched for mushrooms and picked, the sea and a dug pool are tasted before and after a deer dies by it, four days pass, and the carpet waits by a tree in flower on a spring day, on a summer night, and by the pool at a summer dusk
- **THEN** mushrooms are found and picked as known items, the patch is picked clean that day (also after loading) and may fruit the next, the sea is salty, the pool dirty and dirtier by the carcass, which is gone after four days, bees pollinate by the flowers and are drawn near the ground, fireflies are out at night and the bees not, and mosquitoes over the pool bite
