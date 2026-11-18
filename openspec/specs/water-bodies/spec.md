# water-bodies Specification

## Purpose
Defines the water of a created world: a river network from the macro grid's drainage,
lakes at their fill level, terrain shaped by water, water queries and currents, how water
is drawn, and the carpet on water.

## Requirements

### Requirement: River channels
Every macro cell carrying at least 8 m³/s SHALL have a channel to its receiver whose width, depth and speed follow its discharge (width 3·Q^0.5, depth 0.3·Q^0.4, width × depth × speed = Q).

#### Scenario: Channels follow their discharge
- **WHEN** channels for 8, 100 and 2,000 m³/s are sized
- **THEN** width × depth × speed equals the discharge, widths and depths grow with it, speeds stay between 0.5 and 3 m/s, a creek is 7–10 m wide and a great river 100–160 m wide and 4–9 m deep

### Requirement: River network
Channels SHALL run on a smooth meandering line from each cell centre to its receiver's, so tributaries meet them, with a water surface that never rises downstream, and every river SHALL end in the sea or a lake.

#### Scenario: A connected river network
- **WHEN** every channel of a created world is generated
- **THEN** each starts at its cell centre and ends at its receiver's, its pieces join end to end, its surface never rises, its meanders stay near the line between the centres, and following the receivers always ends in the sea or a lake

#### Scenario: Follow a river to the sea
- **WHEN** the longest river is followed piece by piece
- **THEN** the water at every piece is wet, and the river ends in the sea or a lake that has water

### Requirement: Lakes
Lakes SHALL have a flat surface at the level their basin fills to, reaching into neighbouring shore cells but never spilling into a lower outlet.

#### Scenario: Lakes at their fill level
- **WHEN** lake levels are read in lake cells, in cells beside a lake and on the open sea
- **THEN** every lake cell has a level at least its fill level, no lower outlet cell takes the level of the lake above it, and the open sea has no lake

### Requirement: Terrain shaped by water
In a created world, rivers SHALL carve parabolic channels with banks just above the water (never lifting the sea floor), hills SHALL fade into river valleys, and beds and shores SHALL get gravel, mud or sand, identically in generated chunks and the analytic height.

#### Scenario: Rivers carve their beds
- **WHEN** a river's channel and banks are shaped and a chunk it crosses is generated
- **THEN** mid-stream the bed is 1.5 mean depths below the surface, banks stand 0.3 m above the water and fall back to the land, the sea floor is never raised, the chunk's carved bed agrees with the analytic height within 2 m below the water, its bed is mud or gravel with sandy banks, the river shows on its water grids at every stage, and running out of memory is reported

### Requirement: Water queries
The terrain SHALL tell for any place whether the sea, a lake or a river covers it, with the surface, the depth and, in rivers, a current along the channel that is fastest mid-stream and pulls towards the middle; the noise world SHALL have only the sea.

#### Scenario: Water at any place
- **WHEN** water is asked for in a river, the deepest lake, the open sea, dry high land, and the noise world
- **THEN** the river is wet with its level and a current along the ground towards its receiver, the lake has its level and no current, the sea has surface 0 and depth, dry land has none, and the noise world has the sea exactly where the ground is below sea level

### Requirement: Deterministic water
Water SHALL be a pure function of the world bundle, the same on every platform.

#### Scenario: Deterministic water
- **WHEN** water samples across a created world and a generated river chunk are hashed natively and in WebAssembly
- **THEN** the hash matches the recorded golden hash

### Requirement: Water budgets
Generating stages 1 and 2 of a chunk that a river crosses SHALL take at most 8 ms, and building its 4 m water mesh at most 2 ms, in a release build.

#### Scenario: Water budgets
- **WHEN** 24 chunks under river channels are generated and meshed in a release build
- **THEN** the median stage 1–2 time is at most 8 ms and the median water mesh time at most 2 ms

### Requirement: Water drawn
Water SHALL be drawn as translucent surfaces after the opaque scene, coloured by kind, with ripples animated by game time.

#### Scenario: Water is drawn
- **WHEN** a river of a created world is seen from 120 m above
- **THEN** water draw items are submitted, the river shows bluish in the middle of the view, and its pixels change as time passes

### Requirement: Water from Lua
Scripts SHALL read the water at a position (kind, surface, depth, current east and north) and find the nearest river, lake or sea of a created world within a radius.

#### Scenario: Water from Lua
- **WHEN** a script looks for water in the noise world and in a created world
- **THEN** the noise world has no river, unknown kinds are refused, and in the created world a river is found with a channel, a current and its bed below the water, the sea has surface 0, and nothing is found within a metre

### Requirement: The carpet on water
The carpet SHALL float on water surfaces as on the ground and drift with a river's current while it skims the water.

#### Scenario: The carpet rides on water
- **WHEN** the carpet is set down on a river and left alone for two seconds
- **THEN** it drifts on every tick, moves downstream and stays at its clearance above the water
