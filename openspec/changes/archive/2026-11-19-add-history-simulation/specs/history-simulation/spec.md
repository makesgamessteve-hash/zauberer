## ADDED Requirements

### Requirement: Societies rise and fall
The simulation SHALL leave living realms (with capitals) and fallen ones, risings, collapses, wars with battles, battlefields and grudges, great works, and ruins whose causes it made; its history SHALL be told like any other.

#### Scenario: Societies rise and fall
- **WHEN** a world is peopled by Rise and fall
- **THEN** there are living and fallen realms, wars with battlefields and grudges, and their lore is told

### Requirement: The same history from the same seed
The same seed SHALL give the same simulated history.

#### Scenario: The same history from the same seed
- **WHEN** it is simulated twice
- **THEN** the events, polities and wars agree

### Requirement: Kept with the save
A Rise and fall world SHALL keep its whole history with the save and take it back without simulating again; a Settled world keeps none (it is made again from the seed).

#### Scenario: Kept with the save
- **WHEN** the history is saved and loaded, and games of both variants are saved and resumed
- **THEN** it comes back whole, the Rise and fall game takes it back without simulating, and the Settled one saves none
