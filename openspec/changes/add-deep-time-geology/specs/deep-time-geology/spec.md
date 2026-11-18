# Spec Delta: deep-time-geology

## Purpose

Defines the planet's geological past: plate tectonics over deep time with surface
processes, the rock record and its fossils, minerals by geological event, the ley
network, Earth-likeness steering, and the macro grid the later world layers read.

## ADDED Requirements

### Requirement: Lattice
Geology SHALL run on a Fibonacci lattice of 500 to 1,000,000 unit points (40,000 by default) with each point's eight nearest neighbours, nearest first, and a nearest-point lookup that agrees with a brute-force search.

#### Scenario: Even lattice
- **WHEN** a lattice of 3,000 points is built and 4,000 random directions are looked up with and without hints
- **THEN** spacing is within 20 % of the ideal, every nearest neighbour lies within half to one and a half spacings, and every lookup returns the brute-force nearest point

### Requirement: Plate tectonics
Each time step SHALL turn every plate about its Euler pole at an Earth-like angular speed and resolve the boundaries: diverging plates create ocean floor, oceanic crust subducts under buoyant crust and builds volcanic arcs beside trenches, buoyant crusts collide into mountains and weld along sutures, hotspots build tracks on moving plates, and large continents rift apart.

#### Scenario: Plates drift
- **WHEN** a single plate turns at 5 cm/yr for six steps
- **THEN** a marked piece of crust has moved by the expected angle within the lattice's resolution

#### Scenario: Continental collision
- **WHEN** two continental plates converge
- **THEN** collision points lie where they converge, carry metamorphic rock and a time of mountain building, the highest land rises by more than 2 km, and the plates weld into one with a suture

#### Scenario: Subduction builds arcs
- **WHEN** an oceanic plate converges with a continental one
- **THEN** arcs form on the continent with andesite and copper, deep trenches form on the ocean floor beside them, and no other point holds copper

#### Scenario: Spreading ridges
- **WHEN** two oceanic plates diverge
- **THEN** new ocean floor appears where they diverge at about 2,600 m depth, and older floor lies deeper

#### Scenario: Hotspot tracks
- **WHEN** an oceanic plate moves over a hotspot for twelve steps
- **THEN** at least five points carry the hotspot mark, some with basalt, spread over more than four spacings

#### Scenario: Rifting splits a continent
- **WHEN** a continental plate is cut along the equator and its halves drift for four steps
- **THEN** a new plate holds one half, the cut is marked as a rift, a young ocean opens along it, and a plate of one point or a full plate table refuses to split

### Requirement: Surface processes
Land SHALL erode in proportion to its height above the sea, faster under ice, and the eroded rock SHALL settle downhill as strata chosen by environment and era; the sea level SHALL follow the land target with slow cycles and ice ages, and each step SHALL record its era (time, sea level, temperature, humidity, ice age, plates, land share).

#### Scenario: Erosion wears ranges down
- **WHEN** a raised block stands still for ten steps
- **THEN** its top lowers by more than 300 m and new sediment strata appear elsewhere

#### Scenario: Sediments record their environment
- **WHEN** a world of 5,000 points runs for 100 steps
- **THEN** coal only forms in swamps after 360 Myr, salt and gypsum only in salt basins, red sandstone in deserts, till in glacial settings, chalk only after 145 Myr, and the eras run from the past to the present with ice ages in some of them

#### Scenario: Ice ages leave their mark
- **WHEN** a world of land at every latitude goes through a deep ice age and then a warm era
- **THEN** land poleward of 30–52° is glaciated with till and high-latitude land above the sea never escapes the ice, and the warm era glaciates nothing

### Requirement: Rock record and fossils
Every point SHALL keep up to eight strata (rock, environment, thickness, age; youngest on top) above a basement rock without losing thickness when the column is full; collisions SHALL metamorphose strata, and a stratum's fossils SHALL follow from its rock, setting and age.

#### Scenario: Rock record
- **WHEN** strata are added, merged, overflowed, eroded and metamorphosed
- **THEN** similar strata merge, the column keeps its total thickness with the youngest on top, erosion removes from the top, and shale, limestone, sandstone and granite become schist, marble, quartzite and gneiss

#### Scenario: Fossils by age and setting
- **WHEN** fossils are read from strata of different rock, setting and age
- **THEN** marine sediments hold sea creatures, river deposits of 66–250 Myr giant beasts, young coal amber and older coal plants, igneous rock nothing, and some sediments older than 300 Myr mana fossils

### Requirement: Minerals
Mineral deposits SHALL follow geological events: copper, gold and silver in arcs; tin and lapis lazuli in granite collision belts; iron in ancient cratons; salt, gypsum and coal where their strata lie; naphtha and bitumen in thick marine basins; sulfur at active volcanoes; carnelian on lava plateaus; mana crystals on sutures.

#### Scenario: Minerals by geological event
- **WHEN** a world runs to the end
- **THEN** every copper deposit lies on an arc, tin on a collision belt, iron on a craton, salt and coal over their strata, sulfur at an active volcano and mana crystals on a suture, and copper, gold, iron, salt and naphtha all occur

### Requirement: Ley network
Ley nexuses SHALL be chosen among triple junctions, active volcanoes and hotspots, and old sutures at least 0.3 radians apart, and joined by a spanning tree plus about a third of the remaining candidate edges as loops, each a chain of lattice neighbours whose cost is lower along sutures, rifts, hotspot tracks and boundaries.

#### Scenario: Ley network
- **WHEN** the ley network of a finished world is read
- **THEN** it has at least five nexuses kept apart, a spanning tree with loops, paths that chain neighbours from one nexus to the other, and a larger share of path points on old structures than the planet as a whole

### Requirement: Earth-likeness
Each finished history SHALL be scored against its target: Earth has 25–40 % land, 3–8 continents, mountains on every large continent, young and old ranges, under 15 % ice and land in every climate band. Pangaea, Archipelago, Ice world and Hothouse have their own criteria. A missed target SHALL re-run the final quarter with derived seeds up to three times and keep the best result.

#### Scenario: Earth-like worlds
- **WHEN** four seeds of 4,000 points run for 100 steps
- **THEN** each meets every Earth target, with land at 35 % ± 2 and three to eight continents, in at most four attempts

#### Scenario: Other kinds of world
- **WHEN** Pangaea, Archipelago (20 % land), Ice world and Hothouse worlds are made
- **THEN** each meets its target: at most two continents, no landmass above 5 %, at least 15 % ice, and under 2 % ice respectively

#### Scenario: Steering re-runs the final phase
- **WHEN** a world asks for an unreachable 90 % land with two retries
- **THEN** it runs three attempts, restarting the final quarter twice, keeps a full set of eras and reports the target as missed with a score

### Requirement: Macro grid
The final state SHALL be amplified onto a cube-sphere grid of about 6 km cells (at least 16 per face edge) with detail by geology and fitted to the land target by area, then eroded by stream power, hillslope diffusion and lake infill over priority-flood routing, recording heights, lakes, drainage areas and receivers that lead every land cell to the sea.

#### Scenario: Macro grid
- **WHEN** a world with a 40-cell grid finishes with and without eight erosion passes
- **THEN** cell areas sum to the sphere, land covers 35 % ± 1 of it, erosion lowers the land, every land cell drains to the sea with growing drainage, large basins form, heights sample back at cell centres, and neighbours are symmetric except at cube corners

### Requirement: Deterministic and bounded
Geology SHALL be a pure function of the seed and options with the same hash on every platform; it SHALL advance in bounded pieces with progress and stop cleanly when abandoned; a default world SHALL finish within 60 s on a desktop.

#### Scenario: Deterministic everywhere
- **WHEN** the same small world is made twice and with another seed, natively and in WebAssembly
- **THEN** the same seed gives the same recorded golden hash and another seed a different one

#### Scenario: Progress and cancellation
- **WHEN** a world is advanced piece by piece, and another is destroyed half way
- **THEN** progress rises from 0 to 1 through the phases and stays done, and the abandoned world frees everything

#### Scenario: Time budget
- **WHEN** a default world (40,000 points, 125 steps, 524-cell macro grid, 12 passes) runs in a release build
- **THEN** it meets the Earth targets and finishes within 60 s
