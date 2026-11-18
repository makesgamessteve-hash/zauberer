## ADDED Requirements

### Requirement: Motes
Mana SHALL be released as one to 64 motes of one colour around a place, sharing the amount; motes SHALL drift with the local wind, dissolve back into the field within two minutes so that no mana is lost, and number at most 512. Magical creatures SHALL release their motes where they die.

#### Scenario: Motes from the fallen
- **WHEN** the storm bird near the start is struck down
- **THEN** eight storm motes holding its 40 mana appear where it fell

### Requirement: Collecting motes
A mage within 15 m SHALL draw motes in and within 4 m collect them, taking only what fits in the reservoir; what does not fit SHALL stay in the mote.

#### Scenario: Collecting and dissolving
- **WHEN** motes are released in front of the carpet, again when the reservoir is almost full, and far away from it
- **THEN** the carpet collects the first nearly whole, the second only up to the capacity, and the far ones dissolve wholly back into the field

### Requirement: Blooms and channeling
Where a colour is above 60 and near its capacity, the field SHALL now and then gather some of it into motes; a mage moving slower than 5 m/s SHALL be able to channel mana from the field at the channeling rate, each colour by its share there, up to the capacity.

#### Scenario: Blooms and channeling
- **WHEN** a place is made very rich in Fire and looked at for blooms, and the mage channels while still and while flying fast
- **THEN** the place blooms into motes, channeling while still gains mana no faster than the channeling rate, and flying fast gains none
