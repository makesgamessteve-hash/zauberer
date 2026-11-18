# simulation-tiers Specification

## Purpose
Defines the active tier as one whole: its simulations scheduled at their own rates,
coupled through the weather, smoke, wind and water, measured against the budget, and
catching up when chunks that left it come back.

## Requirements

### Requirement: One active tier
The active tier SHALL own heat and fire, surface water and the air, step them at their own rates around the world's focus points, follow a replaced world, and couple them: the weather in use drives all three, fire smokes into the air and leans with its local wind, and standing water puts fire out and soaks its fuel.

#### Scenario: The tier couples its simulations
- **WHEN** lightning strikes a meadow in the tier, the fire burns for a while, and water is then poured over it; and a tier is made without the material registry
- **THEN** the strike is counted and ignites, the fire's smoke is in the air, the water puts the fire out and soaks the fuel, every simulation is timed against the 6 ms budget, and without materials there is no fire and lightning ignites nothing

#### Scenario: The world's weather drives the tier
- **WHEN** the tier uses the world's weather for a second, then a storm the scripts set for four minutes, and then follows another world
- **THEN** it uses the world's weather at the focus point for cloud and the fire's wind, the storm brings lightning, and the other world starts with no chunks

### Requirement: Catch-up
A fire burning in a chunk that leaves the active tier SHALL be remembered; when the chunk returns within ten game minutes the fire SHALL burn on, and later it SHALL have burnt the dry fuel connected to it up to a distance growing with the days gone, scarred as of when the chunk left. A chunk that returns with nothing burning SHALL show its scars until its trees have grown back.

#### Scenario: Fires catch up when their chunk returns
- **WHEN** a meadow fire's chunk leaves and returns at once, and then leaves for 30 game days
- **THEN** the fire burns on after the short absence, and after the month the meadow far from where it was lit is burnt, the scars are dated when the chunk left, the grass has grown back over the ash, the scar is still shown after another absence and gone after 300 days, and fires remembered in another world are forgotten

### Requirement: The tier in the game
The engine SHALL run the active tier every tick and report its cost; scripts SHALL read the milliseconds per simulation (fire, water, air, weather, rigid bodies and mana), the budget, the seconds over budget and the fires remembered.

#### Scenario: Tier from Lua
- **WHEN** a scenario reads the tier's cost, lights a fire, flies the carpet away until the fire's chunk leaves, skips a month and flies back
- **THEN** the budget is 6 ms and a quiet tier costs little, the fire's chunk leaves the tier, and on return there is a burn scar long burnt out with no fire remembered

#### Scenario: The M3 exit demo
- **WHEN** lightning strikes the dry forest by the carpet in a west wind, rain follows, the carpet leaves for a month, and a ring dam on a hillside is filled and breached
- **THEN** the fire runs further downwind than upwind and never burns on water, the rain puts it out, the scar is still there a month later without ash, with ground cover back and no shrubs or trees on it, the pond runs down the slope through the breach, and the water balance holds throughout
