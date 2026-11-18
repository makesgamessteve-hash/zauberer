## MODIFIED Requirements

### Requirement: Saved games
The game SHALL save the world being played under saves/<name>/ (world settings and time, the created world, a player table, the rigid bodies and the region files, each written atomically), list saves, and load a save so that the world, the time, the carpet, the edits and the bodies come back; unknown and invalid names, newer formats and mismatched seeds SHALL be refused with a message.

#### Scenario: Save and continue
- **WHEN** a player removes plants and paints the ground near the carpet, sets the time, saves and quits, and the game starts again
- **THEN** the game continues in the same world at the same time with the carpet where and as it was, the plants stay removed and the ground painted, and a missing save or an invalid name is refused with a message

### Requirement: Edits per chunk
The world SHALL keep the players' edits per chunk (removed plants, painted surface materials, burnt cells dated in game time and changed ground heights), let a later edit of the same plant or cell replace an earlier one, refuse invalid edits, and apply a chunk's edits at the world's time whenever the chunk reaches a stage, so that regenerated chunks come back as they were left and burnt ground heals; a loaded chunk whose ash is due to go or whose plants are due to grow back SHALL be generated again from its surface up.

#### Scenario: Edits are kept per chunk
- **WHEN** edits are recorded for chunks in several regions and applied to a chunk before and after its stages are generated
- **THEN** each chunk returns its edits with repeats replaced, invalid edits are refused, the changed regions are reported until saved, nothing changes before generation, and afterwards the plant is removed and the cell painted exactly once

#### Scenario: Burnt ground heals
- **WHEN** a chunk with burnt cells is generated right after the fire, after 30 game days and after five game years, and a loaded chunk burns and stays loaded for a month and for five years
- **THEN** first the ground is ash and no plant stands on it, after 30 days the ground cover is back but no shrubs or trees and no ash, and after five years every plant is back
