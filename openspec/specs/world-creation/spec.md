# world-creation Specification

## Purpose
Defines how a new world is created from its options, shown as a time-lapse globe,
stored in a world bundle, loaded again, and played on terrain that follows it.

## Requirements

### Requirement: Creation pipeline
World creation SHALL run deep-time geology, then relief, then the present climate, in bounded pieces of work, reporting the stage, a fraction that never decreases, the era in millions of years ago, the time-lapse frames recorded and the Earth-likeness attempt.

#### Scenario: Creation in stages
- **WHEN** a world is created piece by piece
- **THEN** progress passes through deep time, relief and climate in order, its fraction rises from 0 to 1, the era counts down, one time-lapse frame is recorded per deep-time step, and the continents differ between the first and last frames

### Requirement: Cancelling creation
Creation SHALL be cancellable at any point, freeing all its memory, and SHALL not give a world bundle before it is done.

#### Scenario: Cancelling
- **WHEN** creation is cancelled part-way through deep time
- **THEN** asking for its bundle fails before that, and cancelling frees everything it allocated

### Requirement: Creation options
Creation SHALL take a seed, radius, geology target, land fraction, temperature offset, axial tilt and a quality of quick, normal or high that sets the lattice points, steps and macro cell size, and SHALL refuse invalid options with a message.

#### Scenario: Options and quality
- **WHEN** default, quick, high and invalid options are given
- **THEN** the defaults are the default radius, normal quality and a 23° tilt; quick uses 12,000 points and about 12 km cells, normal 40,000 points and about 6 km, high 80,000 points and about 4 km capped at 1,024 cells per face edge; and an unknown quality, an out-of-range tilt or radius are refused

### Requirement: World bundle
A finished creation SHALL give a world bundle with the macro grid, monthly climate, the lattice rock record, ley network, provinces, lineage contacts and eras, queryable by direction for height smooth across cube edges, biome, cell, lattice point and climate.

#### Scenario: The world bundle
- **WHEN** a finished small world's bundle is read
- **THEN** it holds the geology's heights and the climate's biomes, a ley network, provinces and one era per step, lookups by direction agree with the grid, climate lookups are plausible, and heights either side of a cube edge differ by less than 50 m

### Requirement: Bundle files
A world bundle SHALL be written as a versioned file whose sections are compressed and checksummed, read back identically, skip unknown sections, and refuse files that are not bundles, newer, cut short, damaged or missing a section, with a message.

#### Scenario: Files survive the round trip
- **WHEN** a bundle is written and read back
- **THEN** the file starts with ZBWB, is smaller than the raw data, and the read bundle has the same hash, options, metrics, eras, ley network, provinces and contacts

#### Scenario: Damaged files are refused
- **WHEN** a file that is too short, newer, cut short, has a flipped byte, has an extra unknown section or misses a section is read
- **THEN** each is refused with its message except the unknown section, which is skipped, and reading or writing never crashes when memory runs out

### Requirement: Terrain from a created world
A world made from a bundle SHALL take its terrain heights from the bundle's macro grid with hills and crags below the macro scale, and choose surface materials by biome, height, slope and wetness.

#### Scenario: Terrain from the created world
- **WHEN** a world is made from a bundle and its chunks are generated
- **THEN** macro heights stay within 400 m of the bundle's, a sand desert chunk is mostly sand or an ice sheet chunk mostly ice, and the biome rule gives mud below the sea, gravel on scree, snow above the snow line and soil on gentle slopes

### Requirement: Deterministic creation
The same options SHALL create the same world bundle on every platform.

#### Scenario: Deterministic worlds
- **WHEN** a small world is created from seed 42 natively and in WebAssembly
- **THEN** its bundle hash matches the recorded golden hash

### Requirement: New world in the game
The game SHALL offer a new-world screen that creates a world on a worker without stalling frames, shows the stage, percentage and era beside a turning time-lapse globe, flies in the finished world on confirm, and continues in it on the next start.

#### Scenario: A new world from the game
- **WHEN** the player starts a new world, waits until it is finished and confirms
- **THEN** progress never goes back, 80 time-lapse frames are seen for quick quality, the screen says it is ready, the game flies in the created world with its seed, radius and quality, the world's name is saved, and invalid options, names or calls are refused with messages

#### Scenario: The time-lapse globe
- **WHEN** the new-world screen is rendered while deep time runs and after the world is finished
- **THEN** the globe is drawn in the middle of the screen, any recorded frame can be shown, and the finished world is drawn by biome

### Requirement: Places know their past
Scripts SHALL read, for any position in a created world, its biome, monthly temperature and precipitation, its strata down to the basement, its minerals and its biogeographic province with isolation.

#### Scenario: Places know their past
- **WHEN** a script asks about the place where the carpet spawned in a created world
- **THEN** it is over land, has a biome, a rock record ending in the basement, a plausible climate that differs between January and July, a mineral list and a province with isolation between 0 and 1

### Requirement: Saved worlds
A created world SHALL be stored in its save under saves/<name>/ in the user directory and load again later, refusing unknown names and names that could leave the saves directory.

#### Scenario: Saved worlds come back
- **WHEN** the saved world is loaded again, and then an unknown name and a name with a path are loaded
- **THEN** the same world comes back with the same seed and biomes, and the others are refused with a message
