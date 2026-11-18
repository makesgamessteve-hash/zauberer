## ADDED Requirements

### Requirement: Ley lines and nexuses
Each world SHALL have a ley network made from its seed: 64 nexuses on high ground, each with two different colours and a power of 0.5 to 1, joined by ley lines along great circles to their nearest neighbours, and the nearest line and nexus SHALL be found from any place on the planet.

#### Scenario: Ley lines and nexuses
- **WHEN** the network of a world is built twice, and once for another seed, and sampled at a nexus and halfway along a line
- **THEN** the same seed gives the same network, another seed another, the nexuses stand higher than random places on average, a nexus and the middle of a line lie on the network, and a line carries its nexuses' first colours

### Requirement: Emergent sources
The active tier SHALL keep eight colours of mana on 32 m cells whose capacity follows the physical state: Earth from rock, gravel, height and slope; Life from living plants; Light from daylight, the moon, snow and ice; Void from the night, ley lines and nexuses; Storm from wind, height and thunderstorms; Water from water, rain and wet climates; Fire from warmth, burning fuel and fresh scars; Decay from mud, old scars and blight; ley lines add their colours.

#### Scenario: The world shapes the mana
- **WHEN** a chunk is half bare rock and half soil with plants, the time goes from noon to midnight, a thunderstorm with rain comes, the plants are set alight, a pond is poured and a mound is raised
- **THEN** rock holds more Earth and the plants more Life, cells are found full, Light is high by day and Void by night, the storm raises Storm and Water, the fire raises Fire, the pond raises Water and the mound raises Earth

### Requirement: Filling, spreading and ley flow
A cell SHALL fill again towards its capacity over about five minutes, mana released into it SHALL settle back to its capacity, mana SHALL spread slowly between neighbouring cells and fast between cells on a ley line, and cells at a nexus SHALL be rich in its colours and Void.

#### Scenario: Mana fills, spreads and flows along ley lines
- **WHEN** nine tenths of a cell's Earth is drawn and five minutes pass, mana is released into a cell, and the field is sampled at a nexus
- **THEN** the draw takes exactly what was asked, most of the mana is back after five minutes, the neighbours do not gain from it, released mana settles back to the capacity, and at the nexus its first colour and Void are rich

### Requirement: Depletion and blight
Drawing SHALL take mana from the cells within a radius in proportion to what they hold and never more than they hold; draining a cell empty again and again within minutes SHALL blight it for 60 game days, lowering its Life and raising its Decay, recorded as a world edit.

#### Scenario: Drawing depletes and overdrawing blights
- **WHEN** mana is drawn from around a place, invalid draws and draws where nothing is loaded are made, half a cell is drawn, the cell is drained far beyond what it holds again and again, a new field looks at it, and two months pass
- **THEN** the draws take what they can and invalid ones nothing, half a cell does not blight it, the drained cell is blighted with little Life and much Decay and a blight edit, the new field finds it blighted, and after two months it is not

### Requirement: Deterministic mana
The same world and draws SHALL give the same field.

#### Scenario: Deterministic mana
- **WHEN** the same draw and time are run twice
- **THEN** both fields hash the same

### Requirement: Mana in the game
Scripts SHALL list the colours, read a cell, draw and release mana, read the statistics, and read the ley network near any place.

#### Scenario: Mana from Lua
- **WHEN** a scenario reads the mana by the carpet, draws half of its richest colour and more from around, gives some back, and looks up the ley network
- **THEN** there are eight colours in wheel order, the draws are counted, the nearest nexus has two colours and lies where it is said to, and invalid colours, amounts and radii are refused

### Requirement: Mana budget
The mana field over a desktop active tier SHALL cost at most 0.5 ms per 60 Hz tick on average in a release build.

#### Scenario: Mana budget
- **WHEN** the field runs over a full active tier of 384 m with fire, water and air
- **THEN** it costs 0.5 ms per tick or less on average
