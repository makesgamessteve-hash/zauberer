## ADDED Requirements

### Requirement: Cut plants grow back
The world SHALL keep when a plant was cut and its layer; the plant SHALL be gone until its layer has grown back (ground cover in 15 game days, shrubs in a year, trees in five), and a loaded chunk SHALL be generated again when a cut plant is due back.

#### Scenario: Cut plants grow back
- **WHEN** a tree and a herb are cut, edits are checked, and the chunk is looked at just after the cut and after the herb's regrowth
- **THEN** both are gone at first, the herb's return is a turning point and the herb is back after it, the tree is still gone, cut times past 2^16 game minutes are kept, and a cut with an unknown layer is refused
