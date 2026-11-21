## ADDED Requirements

### Requirement: Holding the world
Scripts SHALL be able to hold the world still: while it is held, no game time SHALL pass and the active tier (fire, water, air, creatures) SHALL not advance, while input is sampled and the tick and frame hooks go on.

#### Scenario: The world held and let go
- **WHEN** the world is held for ten ticks and let go
- **THEN** game time does not change while it is held, the scripts' tick hook keeps running (the menu answers), and time goes on once it is let go
