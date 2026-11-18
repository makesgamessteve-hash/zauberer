# vegetation Specification

## Purpose
Defines the plants of the world: a data-driven species registry, where plants grow,
how many and which, the canopy far chunks show, and how forests are drawn from the
ground to the horizon.

## Requirements

### Requirement: Species registry
The game SHALL read plant species from data/flora.lua against the plant atlas: each with a layer (tree, shrub, ground), atlas frames, height and width, a climate niche (temperature and rain ranges, frost limit), the ground it grows on, its steepest slope, whether it needs wet ground, its abundance and a colour per season.

#### Scenario: The species registry
- **WHEN** the base registry is read
- **THEN** it has at least 19 species with trees, shrubs and ground cover, the oak is a tree on soil with a clump impostor, reeds are wetland ground cover, and unknown ids are not found

#### Scenario: Broken registries are refused
- **WHEN** a registry has a missing id, an unknown layer, sprite, clump or material, bad heights, width, niches, density or seasons, a species twice, no species, or an atlas without frames
- **THEN** it is refused with a message naming the species and the field

### Requirement: Where plants grow
A species SHALL grow at a place in proportion to its climate niche (1 at its optimum temperature and rain, 0 at their limits or below its frost limit) and only on its materials, slopes and, if it needs it, wet ground; each biome SHALL carry its own share of trees, shrubs and ground cover.

#### Scenario: Plants grow where their climate suits them
- **WHEN** suitability is asked for temperate, tropical, frosty, steep, rocky and marshy sites, and plants are placed at boreal and tropical latitudes
- **THEN** oaks thrive in the temperate site and not the tropical one, palms the reverse, frost, steepness and rock exclude them, reeds need wet ground, boreal land grows conifers and birch without palms, and tropical land grows no spruce or birch

#### Scenario: Biomes carry their plants
- **WHEN** the cover of biomes is compared
- **THEN** rainforests carry more trees than savannas and savannas more than deserts, steppes more ground cover than trees, and ice and sea none

### Requirement: Plant placement
Chunks SHALL get their plants at stage 3, within the ecology ring of worlds with plants: trees, shrubs and ground cover on jittered grids, never under water, on ground their species take, deterministic, at most 8,192 per chunk, dropped when the chunk is trimmed below stage 3; stage 0 SHALL hold the tree cover every 32 m.

#### Scenario: Plants on every chunk near the player
- **WHEN** a land chunk is brought to stage 3 and a world with plants streams around a focus
- **THEN** it has plants layer by layer on their own ground and never below sea level, generating it again gives the same plants, it stays within its footprint, trimming drops the plants and regenerating restores them, its canopy shows its trees, the focus chunk is targeted at stage 3 while far chunks are not, and a world without plants keeps stage 2

#### Scenario: Deterministic plants
- **WHEN** plants of four land chunks are hashed natively and in WebAssembly
- **THEN** the hash matches the recorded golden hash

### Requirement: Forests at four levels
Plants SHALL be drawn as billboards that face the camera about the ground's up axis and sway in the wind: every plant near the player, trees and shrubs farther, clump impostors from the canopy farther still, and beyond that the canopy as a raised green terrain surface.

#### Scenario: Four levels of forest
- **WHEN** a forested chunk's plants are built for each band, its far mesh is built, and a world with plants is drawn
- **THEN** the near band holds every plant, the mid band all but ground cover, the far band one clump per covered canopy cell, the horizon none, billboards have sizes, atlas frames, species and bounds, some are mirrored, the far mesh is green under the canopy, chunks are drawn in the near and far bands, and all buffers are released

#### Scenario: Plants are drawn
- **WHEN** the default world is rendered around the carpet and a nearby tree is looked at
- **THEN** plant buffers and over 100 billboards are drawn, and the tree's pixels change as it sways

### Requirement: Seasons
Plant colours SHALL follow the season set for the view, blending between spring, summer, autumn and winter per species.

#### Scenario: Seasons colour the plants
- **WHEN** the view's season is set to autumn, then half-way from winter to spring
- **THEN** the frame carries the oak's autumn colour, then the blend of its winter and spring colours, and colours beyond the species stay white

### Requirement: Plants from Lua
Scripts SHALL list the species of the world and the plants within up to 256 m of a position, with species, layer, height and position.

#### Scenario: Plants from Lua
- **WHEN** a script lists the species and the plants around the carpet
- **THEN** it finds trees, shrubs and ground cover in the registry, plants of known species and layers with plausible heights standing on the ground within the radius, none within 0 m, and a radius over 256 m is refused

### Requirement: Vegetation budgets
Placing a chunk's plants (stage 3) SHALL take at most 3 ms, and building its billboards at most 3 ms, in a release build.

#### Scenario: Vegetation budgets
- **WHEN** 24 land chunks get their plants and billboards in a release build
- **THEN** the median stage 3 time and the median billboard time are each at most 3 ms
