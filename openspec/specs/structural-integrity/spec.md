# structural-integrity Specification

## Purpose
Buildings of kit pieces that hold each other up: supports, loads, collapse and fire.

## Requirements

### Requirement: Supports
Every piece of a generated building SHALL be held up from the ground through its supports; nothing falls from a whole building.

#### Scenario: Supports
- **WHEN** every kind of building is checked
- **THEN** every piece is held and settling drops nothing

### Requirement: Collapse
A piece no longer held SHALL fall and lie as rubble; taking a storey's walls brings down what was above.

#### Scenario: Collapse
- **WHEN** a tower's ground-floor walls are taken away
- **THEN** the storeys above and the roof fall to rubble

### Requirement: Overload
A piece carrying more than its material's strength over its cross section SHALL break, and what it held falls.

#### Scenario: Overload
- **WHEN** a thatch post carries a stone slab and a stone wall a floor
- **THEN** the post breaks and the slab falls; the wall holds until taken away

### Requirement: Fire burns timber
Fire burning where a building's timber stands SHALL burn it away and bring down what it held, kept as changes.

#### Scenario: Fire burns timber
- **WHEN** a house's timber is set alight
- **THEN** it burns away and the change is kept
