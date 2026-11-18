# terrain-generation Specification

## Purpose
Generates the planet's terrain chunk by chunk in stages (coarse heights, 1 m
heightfield, surface materials) from the world seed, identically on every platform and
peer, seamlessly across chunk and face borders, and within time and memory budgets.

## Requirements

### Requirement: Deterministic generation
Terrain SHALL be a pure function of the world seed, the planet radius and the chunk: generating a chunk again, in another order, on another thread or on another platform SHALL give bit-identical data.

#### Scenario: Same seed, same terrain
- **WHEN** 16 chunks are generated to stage 2 twice, once in order on one thread and once in reverse order on four worker threads
- **THEN** every chunk's data hash is the same in both runs

#### Scenario: Golden worldgen hashes
- **WHEN** a fixed set of 8 chunks on all six faces is generated to stage 2 with seed 1 on the default planet
- **THEN** the combined hash equals the golden value recorded in the tests, on the Linux, Windows and WebAssembly builds

#### Scenario: Different seeds differ
- **WHEN** the same chunk is generated with seeds 1 and 2
- **THEN** the heightfields differ

### Requirement: Varied relief
The terrain SHALL have oceans, coasts, lowlands, hills and mountains, with land covering 35 % of the surface by default.

#### Scenario: Land fraction
- **WHEN** the terrain height is sampled at 20,000 uniformly distributed directions on the default planet with seed 1
- **THEN** between 30 % and 40 % of the samples are above sea level

#### Scenario: Ocean and mountains
- **WHEN** the same samples are examined
- **THEN** some lie deeper than 2,000 m below sea level and some higher than 1,500 m above it

### Requirement: Chunk stages
Chunks SHALL advance through stage 0 (coarse heights and materials, 16 m apart), stage 1 (heightfield, 1 m apart) and stage 2 (surface materials per 1 m cell); requesting a stage SHALL run the missing earlier stages first.

#### Scenario: Stage dependencies
- **WHEN** stage 2 is requested for a chunk with no data
- **THEN** stages 0 and 1 run first and the chunk reports stage 2

#### Scenario: Stages agree
- **WHEN** a chunk is generated to stage 1
- **THEN** every heightfield sample lies within 6 m of the stage 0 shape interpolated at that point
- **AND** within 0.5 m of the analytic terrain height

### Requirement: Seamless borders
Neighbouring chunks SHALL produce the same heights along their shared edges, including across cube-face seams.

#### Scenario: Shared edges
- **WHEN** a chunk and each of its edge neighbours, within a face and across a face seam, are generated to stage 1
- **THEN** the heights of the shared edge samples differ by less than 1 mm

### Requirement: Surface materials
Every 1 m cell SHALL get one of soil, sand, rock, gravel, snow, ice or mud, chosen from height, slope and latitude.

#### Scenario: Material rules
- **WHEN** the material rule is evaluated for a beach at 1 m, a 45° slope, a meadow at 300 m on the equator, a summit at 4,000 m, polar lowland at 80° latitude and seabed at −100 m
- **THEN** the results are sand, rock, soil, snow, ice and mud

#### Scenario: Materials in a generated chunk
- **WHEN** a mountain chunk and a coastal chunk are generated to stage 2
- **THEN** each contains at least two different materials and no cell is without a material

### Requirement: Generation budget
Generating stages 1 and 2 of a chunk SHALL take at most 5 ms on one core of a mid-range desktop in a release build, leaving room within the 8 ms budget for stages 1–3.

#### Scenario: Measured time
- **WHEN** 32 chunks are generated to stage 2, one after another, in the release build on the Linux CI job
- **THEN** the median time for stages 1 and 2 is at most 5 ms per chunk

### Requirement: Memory per chunk
A chunk SHALL hold at most 2 KB at stage 0, at most 262 KB at stage 1 and at most 330 KB at stage 2, and releasing it SHALL free all of it.

#### Scenario: Footprint by stage
- **WHEN** a chunk is generated stage by stage and then released
- **THEN** its reported size stays within the limits at each stage and no memory is leaked

### Requirement: Co-op behaviour
Terrain SHALL never be sent over the network: each peer generates chunks locally from the world seed it shares with the host.

#### Scenario: Two peers
- **WHEN** two terrain generators are created independently from the same seed and radius
- **THEN** they produce identical chunks

### Requirement: Scripting access
Scripts SHALL be able to query the terrain height and the surface material at a planet position.

#### Scenario: Height and surface from Lua
- **WHEN** a script asks for the height and surface at a position
- **THEN** it receives the height in metres above sea level and the material name
