## MODIFIED Requirements

### Requirement: Defeat
At no health the player SHALL be defeated: controls rest for three seconds, the hunters within 60 m turn away for five minutes, then the player gets up at half health where the carpet first set out, or, with fire or a hunter within 30 m of it, at the nearest place round it clear of both; health and the number of defeats SHALL be saved with the player; unknown harms SHALL be refused.

#### Scenario: Defeat
- **WHEN** the player takes more harm than health is left, waits, and the health is saved and restored; and is defeated again with a fire burning where the carpet set out, and near a wolf
- **THEN** the player is defeated, told so, gets up at half health where the carpet set out (away from the fire, at its height above the ground, and away from the wolf), and the saved health and defeats come back

## ADDED Requirements

### Requirement: Shelter and warnings
More than 1.5 m below the ground (a cellar, a cave) the surface's flames, heat and smoke SHALL NOT hurt the player; standing in flames or on scorching ground the HUD SHALL tell the player to get away from the fire.

#### Scenario: Shelter and warnings
- **WHEN** the player stands in thick smoke and above a fire 3 m below the ground, and then in the flames
- **THEN** below ground nothing hurts; in the flames the player burns and is told to get out of the fire
