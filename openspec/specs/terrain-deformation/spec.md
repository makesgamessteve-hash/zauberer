# terrain-deformation Specification

## Purpose
Defines how the ground's heights change during play: heights kept as edits of the
chunk's 1 m samples, craters and mounds dug or raised across chunk borders, the active
tier's water and fire following the new ground, and deformation from Lua.

## Requirements

### Requirement: Heights as edits
The world SHALL keep changed ground heights as edits of the chunk's 1 m height samples, refuse samples outside the chunk and heights that are not finite or beyond 20 km, and apply them whenever the chunk reaches a stage, updating the chunk's height range.

#### Scenario: Heights are kept as edits
- **WHEN** height edits are recorded for a chunk, including invalid ones, and the chunk is applied before and after its terrain is generated
- **THEN** the invalid edits are refused, nothing changes before generation, and afterwards the samples have their new heights and the chunk's lowest and highest heights include them

### Requirement: Craters and mounds
The world SHALL lower or raise the ground within a radius of a place by a given amount at the centre, falling off smoothly to nothing at the rim, across the borders of loaded chunks with their seams kept closed, remove the plants inside, and report the chunks changed; where the ground is not loaded nothing SHALL change.

#### Scenario: Craters and mounds
- **WHEN** a crater 20 m across is dug 5 m deep next to a chunk border, a mound is raised elsewhere, and a deformation is asked for where no ground is loaded
- **THEN** both chunks change, the bottom is 5 m lower, ground outside the radius is untouched, the shared border heights still agree, the edits are kept, the mound is 3 m higher, and nothing changes where no ground is loaded

### Requirement: The active tier follows the ground
After the ground of a chunk in the active tier changes, its surface water and fire SHALL use the new ground at once.

#### Scenario: A breached dam floods the valley
- **WHEN** a lake fills behind a ridge to below its crest, and then a notch is dug through the crest below the lake's level
- **THEN** the valley beyond stays dry until the breach, afterwards the lake runs through the notch and the valley floods, and no water is gained or lost

### Requirement: Deformation from Lua
Scripts SHALL deform the ground with `world.deform(pos, radius, amount)`, where radius is 0.5 to 64 metres and amount -30 to 30 metres, returning the number of chunks changed, and the active tier SHALL follow the change.

#### Scenario: Deformation from Lua
- **WHEN** a scenario digs a crater near the carpet, raises a mound in the same place, and asks for deformations out of range and where no ground is loaded
- **THEN** the ground is lower by the amount at the centre and untouched beyond the rim, no plant is left inside, the water sees the crater, the mound brings the ground back, out-of-range calls raise errors, and nothing changes where no ground is loaded
