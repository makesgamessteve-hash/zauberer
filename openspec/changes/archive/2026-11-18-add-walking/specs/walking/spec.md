## ADDED Requirements

### Requirement: Stepping off and on
The mage SHALL step off a carpet that is slower than 3 m/s and within 4 m of the floor, onto the ground beside it, and step back on within 3 m of it; a faster or higher carpet SHALL not be left; a save made on foot SHALL come back on foot.

#### Scenario: Stepping off and on
- **WHEN** the mount action is used on a fast carpet, on a slow carpet near the ground, and again on foot beside the carpet, which then climbs
- **THEN** the fast carpet is not left, the mage stands on the ground beside the slow one, steps back on, and flies on

### Requirement: Walking
On foot the mage SHALL walk at 1.6 m/s and run at 5 m/s, turn, jump, fall off edges and follow the ground down slopes, see through the eyes, and be on foot for the creatures and the damage model.

#### Scenario: Walking, running and jumping
- **WHEN** the mage walks for two seconds, runs for one, turns and jumps
- **THEN** the distances match the speeds, the feet stay on the ground, the heading turns, the jump leaves the ground and lands, and the HUD says on foot

### Requirement: Water and ice
Open water deeper than 0.9 m SHALL stop the mage at the shore; ice SHALL be a floor at the water's level.

#### Scenario: Ice holds and deep water stops
- **WHEN** the mage walks towards a pool deeper than a mage can wade, then the pool is frozen and the mage walks on
- **THEN** the mage stops at the shore, and then stands on the ice at its level
