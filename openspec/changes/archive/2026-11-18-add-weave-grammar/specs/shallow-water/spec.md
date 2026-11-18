## MODIFIED Requirements

### Requirement: Volume balance
Every cubic metre SHALL be accounted for: the water on the ground, soaked into it and drained into the terrain's water bodies SHALL equal the water rained and added, up to rounding, counting the water standing in chunks when they left the active tier and the water magic took away.

#### Scenario: Water drains into the sea
- **WHEN** water is poured on a shore that slopes into the sea, and also onto the sea
- **THEN** the sea takes none directly, most of the shore's water drains into it, and the balance holds

## ADDED Requirements

### Requirement: Removing and freezing water
Magic SHALL take standing water away (counted in the balance) and freeze water, standing or a water body, for a time: frozen water SHALL neither flow nor take water in, SHALL be reported as frozen so that it can be walked on, and SHALL melt when its time is up or when thawed.

#### Scenario: Water for magic
- **WHEN** standing water is partly and wholly removed, a pool and the sea are frozen, time passes, the ice is thawed, and frozen water is drawn
- **THEN** removals take what was asked or what there was and are counted in the balance, frozen water keeps its depth and does not flow until it melts on time or is thawed, dry ground does not freeze, the sea freezes at its level, and ice is drawn pale and nearly opaque
