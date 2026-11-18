# planet-coordinates Specification

## Purpose
Defines how every place on the round planet is addressed, converted, related to its
neighbours and turned into precise camera-relative coordinates, so that all world,
simulation, rendering and network code shares one coordinate system.

## Requirements

### Requirement: Cube-sphere chunk grid
The planet SHALL be a sphere of configurable radius (50 km to 6,371 km, default 2,000 km) divided into six cube faces with an area-balancing mapping, each face into N × N chunks with N = round(radius · π/2 / 256 m).

#### Scenario: Default planet size
- **WHEN** a planet with the default radius is created
- **THEN** each face has 12,272 × 12,272 chunks
- **AND** every chunk edge measures between 175 m and 260 m on the sphere

#### Scenario: Radius out of range
- **WHEN** a planet radius of 10 km or 10,000 km is requested
- **THEN** creation fails with an error naming the allowed range

### Requirement: Exact position conversion
A planet position (face, chunk x, chunk y, local offset in metres, altitude) SHALL convert to planet-centred Cartesian coordinates and back, recovering the same chunk and an offset within 1 mm.

#### Scenario: Round trip everywhere
- **WHEN** 10,000 positions spread over all six faces, including chunks on face edges and cube corners, are converted to Cartesian coordinates and back
- **THEN** every position returns to the same chunk with local offset and altitude within 1 mm

#### Scenario: Every direction has one owner
- **WHEN** a direction lies exactly on a cube edge or corner
- **THEN** it converts to exactly one face and chunk, the same on every call

### Requirement: Chunk keys
Each chunk SHALL have a stable 64-bit key that is unique on the planet and identical on every platform.

#### Scenario: Unique keys
- **WHEN** keys are computed for all chunks of a 50 km test planet
- **THEN** no two chunks share a key and every key decodes to its chunk

### Requirement: Neighbours across face seams
The neighbours of a chunk SHALL be the chunks sharing an edge or corner with it on the sphere, including across cube-face seams.

#### Scenario: Crossing a seam
- **WHEN** the eastern neighbour of a chunk on a face's eastern edge is requested
- **THEN** it is a chunk on the adjacent face whose centre is within 1.5 chunk widths of the original chunk's centre
- **AND** asking that chunk for its neighbours returns the original chunk

#### Scenario: Cube corner
- **WHEN** the neighbours of a chunk touching a cube corner are listed
- **THEN** there are exactly 7 distinct neighbours

### Requirement: Local frame and distances
For every position the planet SHALL provide the local up direction, an east/north tangent frame, and the great-circle distance to another position.

#### Scenario: Distance between chunks
- **WHEN** the distance between two positions 1° of arc apart on a 2,000 km planet is measured
- **THEN** it equals 34,906.6 m within 0.1 m

### Requirement: Floating origin
Positions SHALL be convertible to single-precision coordinates relative to an anchor near the camera, with precision depending only on the distance from the anchor.

#### Scenario: Precision far from the zero meridian
- **GIVEN** an anchor at a chunk on the far side of an Earth-size planet
- **WHEN** a point 10 m from the anchor is converted to anchor-relative coordinates
- **THEN** the result is within 0.1 mm of the exact offset

#### Scenario: Curvature below the horizon
- **WHEN** a point at sea level 20 km from the anchor of a 2,000 km planet is converted
- **THEN** its anchor-relative height is 100 m ± 1 m below the anchor's horizontal plane

#### Scenario: Re-anchoring without a jump
- **GIVEN** an object 5 m in front of the camera
- **WHEN** the origin is re-anchored to the camera's chunk 1,100 m away
- **THEN** the object's position relative to the camera changes by less than 1 mm

### Requirement: Scripting access
Scripts SHALL be able to query the planet's radius and chunk count and convert, compare and measure positions.

#### Scenario: Query from Lua
- **WHEN** a script asks for the planet description and the distance between two positions
- **THEN** it receives the radius, chunks per face edge and the distance in metres
