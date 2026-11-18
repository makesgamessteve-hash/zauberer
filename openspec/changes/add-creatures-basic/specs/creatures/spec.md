## ADDED Requirements

### Requirement: Species
Species SHALL be data: an id, length, mass, walking and running speed, whether they fly and at what height, diet, whether they keep in packs, sight, hearing and smell, health, aggression, fear of fire and of the player, resistance to each harm, and for magical ones their mana's colour and motes. A later definition with the same id SHALL replace the earlier one; values out of range and more than 32 species SHALL be refused.

#### Scenario: Species are data
- **WHEN** species are defined, looked up, redefined and defined with bad values or beyond the limit, and an unknown species is spawned
- **THEN** each gets its index, lookups find them or nothing, a redefinition replaces the old values, bad values and the 33rd species are refused, and no creature of an unknown species is made

### Requirement: Moving
Walkers SHALL keep to the ground, stand on ice and turn away from open water; fliers SHALL keep their cruising height above the ground; a creature outside the active tier SHALL wait where it is.

#### Scenario: On the ground, in the air, out of the water
- **WHEN** deer and a storm bird move about a meadow with a pond for a minute, a deer is placed on the frozen pond, and another far outside the active tier
- **THEN** the deer stay on the ground and out of the water and wander, the bird keeps above the ground, the deer on the ice stands at its level, and the far one waits without moving

### Requirement: Senses
Each creature SHALL sense the player within its sight (shorter at night) or close hearing, fire within two rings around it, scent on the air, and the noises, lights and threats the scripts raise; a light seen in the dark SHALL look like fire to it.

#### Scenario: Scent on the wind
- **WHEN** scent drifts past a wolf for ten seconds
- **THEN** it goes to investigate

### Requirement: Choice
Five times a second, staggered, each creature SHALL choose by utility between wandering, grazing, resting, fleeing, hunting the player and investigating: fear of fire, threats, noises and a nearby player makes it flee away from them, a hungry hunter goes for a player on foot (much less for one on the carpet) and attacks when close, a carnivore follows a scent upwind, and packs keep together.

#### Scenario: Fear of fire, hunters and the player
- **WHEN** a fire starts near a deer, a wolf is near a mage on foot who then rises high on the carpet, and a deer hears a loud noise and sees threats
- **THEN** the deer flees away from the fire and its flight is told once, the wolf hunts and attacks the mage on foot but gives up on the carpet, and the noise and threats make the other deer flee

### Requirement: Harm and death
Creatures SHALL be hurt by flames and heat, thick smoke and fast bodies, and by the scripts with a kind of harm, less their resistance to it; healing SHALL not exceed full health; pain SHALL frighten them; at no health a creature SHALL die and the death SHALL be told with its cause, and for a magical creature its colour and motes.

#### Scenario: Harm and death
- **WHEN** a deer is hurt and healed, a storm bird resisting smoke breathes it and is struck down, a deer stands in a fire, and a boulder falls on a wolf
- **THEN** health drops and returns to full, pain frightens, the bird takes half the smoke, its death is told with the cause and it is magical, the dead feel nothing more, the fire burns the deer and the boulder hurts the wolf

### Requirement: Deterministic creatures
The same seed, species, creatures and world SHALL give the same creatures after the same time; another world SHALL start with none.

#### Scenario: Deterministic creatures
- **WHEN** a dozen creatures of three species live five seconds around a mage twice, and the creatures then follow the world anew
- **THEN** both runs hash the same and every creature is visited, and none are left

### Requirement: Creatures in the game
The active tier SHALL own the creatures and step them every tick; scripts SHALL define species, spawn, read, find, hurt, heal and remove creatures, raise stimuli, tell where the player is, and read events and counts; the game SHALL load the species from data, place some near the start, and draw them.

#### Scenario: Creatures from Lua
- **WHEN** a scenario defines a species, spawns creatures of the base species by the carpet, lights a fire by deer, hurts a storm bird to death and reads the events and counts, and errors are provoked
- **THEN** the creatures stand on the ground or fly, the deer flee the fire, the bird's death brings its storm motes, the counts add up, and bad arguments are refused

#### Scenario: Creatures are drawn
- **WHEN** walkers and fliers, living and dead, are drawn near the eye and far from it
- **THEN** those near are drawn with legs or wings in their colour and those beyond the range are not

### Requirement: Creature budget
Two hundred creatures in a desktop active tier SHALL cost at most 0.5 ms a tick on average in a release build on Linux.

#### Scenario: Creature budget
- **WHEN** 200 wolves, deer and birds live around a mage on foot for five seconds after a noise
- **THEN** most are in the active tier and a tick costs at most 0.5 ms on average
