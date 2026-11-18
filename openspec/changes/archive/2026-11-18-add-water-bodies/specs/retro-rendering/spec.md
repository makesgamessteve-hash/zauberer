# Spec Delta: retro-rendering

## MODIFIED Requirements

### Requirement: Water
Chunks SHALL get translucent water surfaces where the sea, a lake or a river covers them (the sea exactly at sea level), drawn after the opaque scene in a mesh of their own.

#### Scenario: Water at sea level
- **WHEN** a sea chunk's terrain and water meshes are built at near and far levels
- **THEN** the terrain mesh has no water, and the water mesh covers the chunk with translucent vertices exactly at sea level (within 1 cm) above the sea floor, facing up
