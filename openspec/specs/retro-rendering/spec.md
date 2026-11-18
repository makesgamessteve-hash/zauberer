# retro-rendering Specification

## Purpose
Defines the game's look and how the streamed planet is drawn: palette quantisation with
ordered dithering, sky and fog, terrain levels of detail with exact curvature and
water, mesh and frame budgets, the render origin, and their scripting access.

## Requirements

### Requirement: Palette quantisation
When the palette is on, every scene pixel SHALL be one of the palette's colours, chosen through the 32³ lookup table after an 8 × 8 ordered dither; text SHALL be drawn after quantisation; the palette SHALL be switchable off.

#### Scenario: Palette colours only
- **WHEN** the terrain scene is rendered with the palette on
- **THEN** every pixel of the internal image is one of the 64 palette colours

#### Scenario: Palette off
- **WHEN** the palette is switched off
- **THEN** the scene keeps colours outside the palette

#### Scenario: Ordered dithering
- **WHEN** a colour halfway between two neighbouring colours of a ramp is quantised over an 8 × 8 block
- **THEN** both colours appear, and the pattern repeats every 8 pixels

### Requirement: Sky and fog
The renderer SHALL draw a gradient sky with a sun whose horizon dips with the camera's altitude, and SHALL blend the scene into the horizon colour with distance fog that reaches 98 % at the configured fog distance.

#### Scenario: Sky gradient and sun
- **WHEN** the sky colour is evaluated horizontally, straight up, towards the sun and slightly below the horizontal at 1,000 m altitude over a 2,000 km planet
- **THEN** the colours are the horizon colour, the zenith colour, the sun colour and a colour between them (the horizon dipping by 1.8°)

#### Scenario: Fog hides the distance
- **WHEN** fog is set to 12,000 m
- **THEN** fog is 98 % at 12,000 m and below 5 % at 1,000 m

### Requirement: Terrain levels of detail
Every loaded chunk SHALL be drawn with a grid of 4, 8, 16, 32 or 64 m chosen by its distance to the camera, without visible cracks between levels.

#### Scenario: Levels of detail
- **WHEN** chunk meshes are built for each level
- **THEN** they have 65², 33², 17², 9² and 5² grid vertices, only the 4 m level is textured, and chunks at 300 m, 1 km, 3 km, 6 km and 10 km get levels 0 to 4

#### Scenario: No cracks between levels
- **WHEN** a chunk mesh is built
- **THEN** every edge vertex has a skirt vertex below it, and all grid triangles face up

### Requirement: Curvature
Terrain SHALL follow the planet's curvature exactly: each chunk is drawn in its own tangent frame and placed relative to the render origin.

#### Scenario: Curvature
- **WHEN** a chunk 20 km from the render origin is placed
- **THEN** its centre lies d²/2R below the origin's horizontal plane within 1 m, and its up direction is tilted by d/R

### Requirement: Water
Chunks SHALL get translucent water surfaces where the sea, a lake or a river covers them (the sea exactly at sea level), drawn after the opaque scene in a mesh of their own.

#### Scenario: Water at sea level
- **WHEN** a sea chunk's terrain and water meshes are built at near and far levels
- **THEN** the terrain mesh has no water, and the water mesh covers the chunk with translucent vertices exactly at sea level (within 1 cm) above the sea floor, facing up

### Requirement: Mesh and frame budgets
Terrain meshes SHALL be built within a per-frame budget, nearest first; with the desktop profile at 700 m altitude the terrain SHALL take at most 4,000 draw items, 1,000,000 triangles and 3 ms of main-thread CPU per frame.

#### Scenario: Mesh build budget
- **WHEN** a camera starts over unstreamed terrain with budgets of 2 fine and 10 coarse meshes
- **THEN** no frame builds more than 12 meshes, and once streaming settles every loaded chunk has a mesh and released chunks have none

#### Scenario: Frame budgets
- **WHEN** the desktop profile has settled at 700 m altitude, in the release build on the Linux CI job
- **THEN** the terrain uses at most 4,000 draw items and 1,000,000 triangles, and the terrain view takes at most 3 ms per frame (median)

### Requirement: Render origin
The world SHALL move the render origin to the first player's chunk when the player is more than 1,024 m away horizontally, and terrain SHALL be re-placed relative to the new origin.

#### Scenario: Origin follows the player
- **WHEN** the first focus moves 3 km
- **THEN** the origin moves to its chunk, the camera is above the new origin, and every loaded chunk is drawn

### Requirement: Scripting access
Scripts SHALL be able to set the atmosphere and the palette, convert between planet positions and render coordinates, get the local up direction and set the view distance.

#### Scenario: Terrain scene
- **WHEN** a script streams terrain around a mountain, sets the atmosphere and the palette and looks at the summit
- **THEN** the rendered image matches the golden image on llvmpipe and the statistics report terrain draws
