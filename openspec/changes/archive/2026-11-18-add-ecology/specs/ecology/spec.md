## ADDED Requirements

### Requirement: Populations per chunk
Each ecology species SHALL have a count per chunk that grows logistically towards a capacity given by its density and how well the chunk's habitat (forest, open, wetland, dry, cold) suits it; counts SHALL stay between zero and the capacity, catch up lazily in steps of at most a day and at most two years at once, and die out where the land holds none.

#### Scenario: Populations per chunk
- **WHEN** capacities are computed for forest and desert, a nearly empty forest chunk is advanced ten years, a desert chunk with deer is advanced, and nothing passes
- **THEN** the forest holds deer and the desert none, the deer grow back past half their capacity, the desert's die out, no time changes nothing, and every count stays within its bounds

### Requirement: The food web
Predators SHALL eat the counts of the species they hunt (data: eats), and their own capacity SHALL fall towards a tenth when their prey are gone.

#### Scenario: The food web
- **WHEN** chunks with and without wolves are advanced half a year, and a chunk with wolves but no prey three years, and a chunk an absurdly long time
- **THEN** wolves keep deer and hares down, starving wolves fall below a third of their capacity, and every count stays bounded

### Requirement: Migration
Species SHALL flow between neighbouring chunks from where they are dense for the capacity to where they are thin, keeping the total, never above a capacity and never into land that holds none.

#### Scenario: Migration
- **WHEN** a full and an empty forest chunk, and a full chunk and a desert, exchange for a season
- **THEN** deer move into the empty chunk with the total kept, and none move into the desert

### Requirement: Virtualisation
The chunks around the player SHALL spawn individuals from their counts: groups for herds and packs, at places fixed per chunk, species, slot and day, never in deep water; when the player leaves, the individuals SHALL fold back into the counts with the dead taken off.

#### Scenario: Virtualisation
- **WHEN** the carpet waits in the default world, the hares of its chunk are killed, the carpet flies away, comes back a year later, and the animals are folded and spawned again the same day
- **THEN** the chunks around it have their animals of the right species, about as many as the counts, the hares' count drops by those killed and the dead are gone, a year later they have bred back, and the same animals come back in the same places

### Requirement: Ecology in the game
The game SHALL keep the populations with the game (the dead among the live individuals taken off), keep them when flying on in the same world, start anew in another world, and leave the fixed groups near the start as they are.

#### Scenario: Ecology in the game
- **WHEN** a habitat is read in the default world, the counts of every chunk seen are checked, and the populations are saved and restored
- **THEN** deer and hares can live there and cranes are few, every count is within its capacity, and the restored counts are the same
