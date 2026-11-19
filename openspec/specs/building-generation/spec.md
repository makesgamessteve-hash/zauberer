# building-generation Specification

## Purpose
Buildings generated in layers from their seed (program, massing, floor plan, kit pieces, furnishing, contents), walked into seamlessly; set pieces for story places.

## Requirements

### Requirement: Programs
A building type SHALL define required and optional rooms with shares and the room each opens off; the household's trade and wealth SHALL choose the optional ones; caravanserais have twenty rooms and more.

#### Scenario: Programs
- **WHEN** programs are made for a weaver, the wealthy and a caravanserai
- **THEN** the weaver has a loom room, the wealthy sleep upstairs, the caravanserai has 20+ rooms, and an unknown type is refused

### Requirement: Massing by style and land
Massing SHALL take footprint (on the 0.5 m grid), storeys, roof and wall material from the style family weighted by the land's climate and materials, with courtyards, iwans and windcatchers where the family has them.

#### Scenario: Massing by style and land
- **WHEN** buildings are massed for upland forest, desert coast, steppe and a tower
- **THEN** materials and roofs follow the land, tents are felt yurts, towers domed and tall

### Requirement: Floor plans
Every room SHALL be reachable from the front door (through doors and stairs), at least 1.4 m across and inside the footprint; courtyard buildings ring their court, big ones have a corridor, storeys are joined by stairs.

#### Scenario: Floor plans
- **WHEN** every building type is planned
- **THEN** every room is reached, none too small or outside, caravanserais have a court and iwan, big buildings a corridor, towers stairs

### Requirement: Kit pieces
Buildings SHALL be built of pieces each with a material: wall segments of at most 2 m broken by doors and windows, floors per room, roofs, stairs, plinths or stilts; open structures (wells, docks) have no rooms.

#### Scenario: Kit pieces
- **WHEN** a house, a well, a dock, a stilt house and a plinthed house are built
- **THEN** the pieces are as described for each

### Requirement: Furnishing
Furniture SHALL stand inside its room, not overlapping, against walls with clear floor before it, groups together, a light in each room (lamps on furniture), containers marked, matched to the household's trade and wealth; furnishing happens once.

#### Scenario: Furnishing
- **WHEN** a weaver's and a smith's houses are furnished
- **THEN** a hearth, bed and loom for the weaver, a forge for the smith, lights and containers, no overlaps

### Requirement: Contents on opening
A container's contents SHALL be the same each time it is made, by the household (books in bookcases, trade goods, coins for the wealthy), less what was taken.

#### Scenario: Contents on opening
- **WHEN** containers are opened twice
- **THEN** they hold the same things

### Requirement: Seamless interiors
The walker SHALL be stopped by walls, windows and closed doors, stand on floors and stairs, walk in through an opened door; doors and taken things SHALL be kept as changes with the save.

#### Scenario: Seamless interiors
- **WHEN** the heir walks at a closed front door, opens it, walks in and on, takes from a container and the game is saved
- **THEN** the door stops them, then they are inside on the floor, the back wall holds, the things are taken once, and the open door is in the save
