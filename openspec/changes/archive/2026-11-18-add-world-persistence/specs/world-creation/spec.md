## MODIFIED Requirements

### Requirement: Saved worlds
A created world SHALL be stored in its save under saves/<name>/ in the user directory and load again later, refusing unknown names and names that could leave the saves directory.

#### Scenario: Saved worlds come back
- **WHEN** the saved world is loaded again, and then an unknown name and a name with a path are loaded
- **THEN** the same world comes back with the same seed and biomes, and the others are refused with a message
