## MODIFIED Requirements

### Requirement: The tier in the game
The engine SHALL run the active tier every tick and report its cost; scripts SHALL read the milliseconds per simulation (fire, water, air, weather, rigid bodies, mana and creatures), the budget, the seconds over budget and the fires remembered.

#### Scenario: Tier from Lua
- **WHEN** a scenario reads the tier's cost, lights a fire, flies the carpet away until the fire's chunk leaves, skips a month and flies back
- **THEN** the budget is 6 ms and a quiet tier costs little, the fire's chunk leaves the tier, and on return there is a burn scar long burnt out with no fire remembered

#### Scenario: The M3 exit demo
- **WHEN** lightning strikes the dry forest by the carpet in a west wind, rain follows, the carpet leaves for a month, and a ring dam on a hillside is filled and breached
- **THEN** the fire runs further downwind than upwind and never burns on water, the rain puts it out, the scar is still there a month later without ash, with ground cover back and no shrubs or trees on it, the pond runs down the slope through the breach, and the water balance holds throughout
