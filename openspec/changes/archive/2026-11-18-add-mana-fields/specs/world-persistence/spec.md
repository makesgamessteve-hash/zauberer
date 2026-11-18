## MODIFIED Requirements

### Requirement: Edits per chunk
The world SHALL keep the players' edits per chunk (removed plants, painted surface materials, burnt cells dated in game time, changed ground heights and blighted mana cells), let a later edit of the same plant or cell replace an earlier one, refuse invalid edits, and apply a chunk's edits at the world's time whenever the chunk reaches a stage, so that regenerated chunks come back as they were left and burnt ground heals; a loaded chunk whose ash is due to go or whose plants are due to grow back SHALL be generated again from its surface up.

#### Scenario: Edits are kept per chunk
- **WHEN** edits are recorded for chunks in several regions and applied to a chunk before and after its stages are generated
- **THEN** each chunk returns its edits with repeats replaced, invalid edits are refused, the changed regions are reported until saved, nothing changes before generation, and afterwards the plant is removed and the cell painted exactly once

#### Scenario: Burnt ground heals
- **WHEN** a chunk with burnt cells is generated right after the fire, after 30 game days and after five game years, and a loaded chunk burns and stays loaded for a month and for five years
- **THEN** first the ground is ash and no plant stands on it, after 30 days the ground cover is back but no shrubs or trees and no ash, and after five years every plant is back
