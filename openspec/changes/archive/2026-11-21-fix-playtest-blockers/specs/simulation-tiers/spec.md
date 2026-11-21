## ADDED Requirements

### Requirement: Lightning keeps its distance
Natural lightning SHALL strike 100 .. 355 m from the player (the highest of a few places at that distance), so a storm's fires start where the player can see them coming.

#### Scenario: Lightning keeps its distance
- **WHEN** a storm the scripts set sends its first lightning
- **THEN** nothing burns within 90 m of the player
