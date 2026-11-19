# settlements Specification

## Purpose
Settlements laid out near the heirs (streets, plots, roles, walls, land use, terrain fit, special places, ruins) and the M8 exit demo.

## Requirements

### Requirement: Places come to be near the heirs
Settlements within 3 km of any heir SHALL be laid out and their buildings massed; buildings within 120 m planned and built; within 35 m furnished; places beyond 5 km forgotten, their changes kept. With several heirs, around each.

#### Scenario: Places come to be near the heirs
- **WHEN** the heir stands in Anarvan, next to a building, then far away
- **THEN** Anarvan is laid out and massed, the building is furnished, and far away the place is forgotten

### Requirement: Streets, plots and roles
A settlement SHALL have a centre (square, market or harbour), main streets towards its roads and ring streets, plots along them that do not overlap, wells on the square, roles near the centre, smithies at the edge; Anarvan is a village of weavers.

#### Scenario: Streets, plots and roles
- **WHEN** Anarvan and a market town are laid out
- **THEN** both have their streets, the well on the square, nothing on the square, weavers in Anarvan, and the town's bazaar nearer the centre than its smithy

### Requirement: Walls, gates and land use
Walled places SHALL have a ring wall with towers and a gate where each main street leaves, all buildings within; land use SHALL lie around in sectors, fields with crops by climate; a city's citadel stands on its high ground.

#### Scenario: Walls, gates and land use
- **WHEN** a walled city is laid out
- **THEN** it has walls, towers and gates, its buildings within, fields and pastures round about and a citadel of 100 rooms and more

### Requirement: Terrain fit
Buildings SHALL stand on plinths on slopes (terraced where steep) and on stilts in stilt villages and over water; steep streets SHALL have steps.

#### Scenario: Terrain fit
- **WHEN** a stilt village and a village on a steep slope are laid out
- **THEN** the stilt village is on stilts, the slope village has terraced buildings and stepped streets

### Requirement: Special places
A caravanserai SHALL be one courtyard building and a well; a nomad camp tents round a fire with pastures; a mage tower a tower with outbuildings; cliff dwellings rows of carved houses along the steepest slope; a citadel a castle.

#### Scenario: Special places
- **WHEN** each special place is laid out
- **THEN** each has its plan

### Requirement: Ruins
Ruined places SHALL decay by their age and cause: roofs, upper floors, doors and many walls gone (the rest settled by integrity), most furniture looted, overgrown and buried by the land.

#### Scenario: Ruins
- **WHEN** a 300-year-old ruin is entered
- **THEN** it is roofless, fallen and looted, with its cause, overgrowth and burial

### Requirement: The M8 exit demo
The heir SHALL be able to fly into a generated oasis town and enter a house, trade and get directions, rest in a caravanserai, enter the palace of a citadel of 100+ rooms, read one war in two chronicles from its two sides and descend into a karst cave with an underground river.

#### Scenario: The M8 exit demo
- **WHEN** the exit demo is played through
- **THEN** each part of it happens in the world as generated
