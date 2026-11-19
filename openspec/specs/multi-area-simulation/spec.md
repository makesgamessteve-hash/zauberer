# multi-area-simulation Specification

## Purpose
Several active simulation areas, one per group of heirs, sharing the host's budget.

## Requirements

### Requirement: Areas share the budget
The radius simulated around each focus SHALL be settable for fire, water, air and mana together (50 to 2000 m from scripts), the old one returned.

#### Scenario: Areas share the budget
- **WHEN** the tier's radius is changed with and without fire
- **THEN** it changes and the old one is told

### Requirement: An area for each group of heirs
Heirs within 2 km SHALL share an area and others get their own, at most four; the radius SHALL be 300 m over the square root of the areas, smaller when over budget, at least 80 m; each area gets a focus and the rest are cleared.

#### Scenario: An area for each group of heirs
- **WHEN** heirs in two groups and five apart are placed
- **THEN** two areas, then four, with radii as described and foci set and cleared
