# world-persistence Specification

## Purpose
Defines how the game remembers: the players' edits kept per chunk on top of the
generated world, region files that store them, saved games under saves/<name>/ that
continue where the player left off, and the migration of saves from earlier versions.

## Requirements

### Requirement: Edits per chunk
The world SHALL keep the players' edits per chunk (removed plants, painted surface materials, burnt cells dated in game time, changed ground heights and blighted mana cells), let a later edit of the same plant or cell replace an earlier one, refuse invalid edits, and apply a chunk's edits at the world's time whenever the chunk reaches a stage, so that regenerated chunks come back as they were left and burnt ground heals; a loaded chunk whose ash is due to go or whose plants are due to grow back SHALL be generated again from its surface up.

#### Scenario: Edits are kept per chunk
- **WHEN** edits are recorded for chunks in several regions and applied to a chunk before and after its stages are generated
- **THEN** each chunk returns its edits with repeats replaced, invalid edits are refused, the changed regions are reported until saved, nothing changes before generation, and afterwards the plant is removed and the cell painted exactly once

#### Scenario: Burnt ground heals
- **WHEN** a chunk with burnt cells is generated right after the fire, after 30 game days and after five game years, and a loaded chunk burns and stays loaded for a month and for five years
- **THEN** first the ground is ash and no plant stands on it, after 30 days the ground cover is back but no shrubs or trees and no ash, and after five years every plant is back

### Requirement: Region files
Edits SHALL be stored in region files of 16 × 16 chunks with a magic, a format version, a checksum and compressed data, and reading SHALL refuse foreign, newer, truncated or damaged files with a message and report running out of memory.

#### Scenario: Region files
- **WHEN** a region's edits are written, read back into an empty store, and then read from damaged copies and with failing allocations
- **THEN** exactly that region's chunks come back with the same edits and not marked as changed, and each damaged copy is refused with its own message

### Requirement: Saved games
The game SHALL save the world being played under saves/<name>/ (world settings and time, the created world, a player table, the rigid bodies and the region files, each written atomically), list saves, and load a save so that the world, the time, the carpet, the edits and the bodies come back; unknown and invalid names, newer formats and mismatched seeds SHALL be refused with a message.

#### Scenario: Save and continue
- **WHEN** a player removes plants and paints the ground near the carpet, sets the time, saves and quits, and the game starts again
- **THEN** the game continues in the same world at the same time with the carpet where and as it was, the plants stay removed and the ground painted, and a missing save or an invalid name is refused with a message

### Requirement: Older saves load
Saves written by earlier formats SHALL keep loading: format 1 through a fixture kept in the repository, and created worlds kept before saves existed (worlds/<name>.zbw) by moving them into a save when they are loaded.

#### Scenario: Saves of an earlier version still load
- **WHEN** the format 1 fixture save is copied into an empty user folder and the game starts
- **THEN** it continues that game with its time, carpet and edits

#### Scenario: Worlds from before saves move into a save
- **WHEN** a created world exists only as worlds/<name>.zbw and the game starts in it
- **THEN** the same world is played, a message says it was moved, and saves/<name>/ holds its world settings and bundle
