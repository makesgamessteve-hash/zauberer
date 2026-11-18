# thermal-and-fire Specification

## Purpose
Defines heat and fire in the active tier: temperature above the ambient air, fuel and
its moisture per 2 m cell, ignition, spread with wind and slope, embers, what stops a
fire, and the burn scars it leaves in the world.

## Requirements

### Requirement: Heat and fuel in the active tier
Every loaded chunk with its plants within the active radius of a focus point SHALL be simulated as 2 m cells holding a temperature above the ambient air, fuel with its material from the registry, and the fuel's moisture; chunks with nothing above ambient SHALL sleep, and places outside the simulation SHALL report that they are not loaded.

#### Scenario: Heat without fire
- **WHEN** a little heat is added to a meadow, time passes, then a lot of heat is added, and heat or fire is asked for outside the active tier
- **THEN** the cell warms without burning and cools back to the air's temperature with every chunk asleep, the lot of heat sets it burning, and outside the tier nothing is loaded or ignited

### Requirement: Fire spreads
A dry cell SHALL ignite at its fuel's ignition temperature after its water has boiled off, burn its fuel at the material's rate and give a share of its heat to its neighbours, more downwind and uphill; burnt-out cells SHALL be recorded in the world's deltas, which turn the ground to ash and remove the plants on it.

#### Scenario: Fire spreads and burns out
- **WHEN** a flat, windless meadow is set alight in the middle and left to burn
- **THEN** a roughly round fire spreads with flames hotter than 500 deg C, burns out, and leaves burnt cells that the world records with ash on the ground

#### Scenario: Wind and slope lean the fire
- **WHEN** fires are lit on a meadow with wind from the west and on a meadow sloping up to the east
- **THEN** each spreads at least twice as far downwind as upwind, and further uphill than downhill

### Requirement: Fire stops
Water (sea, lakes and rivers), bare ground and fuel wetter than its extinction moisture SHALL NOT burn, and rain SHALL wet the fuel and put burning cells out.

#### Scenario: Water and rain stop fire
- **WHEN** a meadow with a channel of water burns, and another fire is rained on heavily
- **THEN** the fire burns up to the channel but not beyond it, and in the rain every cell stops burning and the wetted fuel cannot be lit

### Requirement: Embers
Crown fires in wind SHALL throw embers downwind, which start spot fires where they land on dry fuel.

#### Scenario: Embers start spot fires
- **WHEN** a forest burns in a strong wind
- **THEN** embers are thrown and some of them start spot fires

### Requirement: Deterministic fire
The same fuel, weather and ignitions SHALL give the same fire, cell for cell.

#### Scenario: Deterministic fire
- **WHEN** the same forest is lit twice in the same wind
- **THEN** the simulation hashes are equal after a minute

### Requirement: Fire in the game
The engine SHALL run the fire at 20 Hz around the world's focus points and draw flames on burning cells near the camera; scripts SHALL ignite, heat, read cells and statistics, and set the wind and rain.

#### Scenario: Fire from Lua
- **WHEN** a scenario sets the wind, lights the ground near the carpet, waits and reads the fire
- **THEN** cells burn and burn out downwind, the statistics follow, the flames are drawn, and invalid radii, heat and weather are refused

### Requirement: Fire budget
A chunk full of fire SHALL step in at most 2 ms on desktop in a release build.

#### Scenario: Fire budget
- **WHEN** a forest chunk burns across its width
- **THEN** the median step takes at most 2 ms

### Requirement: Fire and the air
Burning cells SHALL release smoke into the air at their burn rate, and fire SHALL lean with the local wind of its chunk.

#### Scenario: Fire smokes and follows the local wind
- **WHEN** a fire burns in a forest chunk that shelters the wind
- **THEN** smoke is released into the air above it, and the fire leans with the chunk's sheltered wind rather than the weather's
