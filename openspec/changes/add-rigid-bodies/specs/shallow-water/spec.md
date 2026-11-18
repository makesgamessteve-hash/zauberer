## MODIFIED Requirements

### Requirement: Water flows
Surface water in the active tier SHALL flow between neighbouring 2 m cells by the difference of their water surfaces, never more than a cell holds, so that it runs downhill, spreads, and fills hollows to a flat surface; each cell SHALL report the velocity of its water from the flows through its faces.

#### Scenario: Water runs downhill
- **WHEN** water is poured high on a rock slope
- **THEN** the water running down the slope reports a velocity downhill, the place it was poured drains and the water gathers further down, with the chunk awake while it moves

#### Scenario: A basin fills flat and goes still
- **WHEN** water is poured into a rock basin and left
- **THEN** its surface levels out within 2 cm, all of it stays in the basin, and the chunk goes to sleep keeping the water

### Requirement: Volume balance
Every cubic metre SHALL be accounted for: the water on the ground, soaked into it and drained into the terrain's water bodies SHALL equal the water rained and added, up to rounding, counting the water standing in chunks when they left the active tier.

#### Scenario: Water drains into the sea
- **WHEN** water is poured on a shore that slopes into the sea, and also onto the sea
- **THEN** the sea takes none directly, most of the shore's water drains into it, and the balance holds
