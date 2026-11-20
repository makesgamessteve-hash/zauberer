## ADDED Requirements

### Requirement: Starting the game over
The reset key (F9) SHALL ask first and, pressed again within five seconds, start the game over as at its first start: every saved game deleted, only the settings kept in the save data, the configuration unchanged, nothing of the old game saved, and the world, simulation and scripts made anew in the same window. It SHALL be refused, with the reason shown, in a co-op session and in tests and scenarios.

#### Scenario: The reset key asks first
- **WHEN** the reset key is pressed once, then again within five seconds, then once more after the time ran out
- **THEN** the first press asks, the second starts over (or says why it cannot), and the third asks again

#### Scenario: Reset to the first start
- **WHEN** a game with a saved world, an old world file and settings is played and the reset key pressed twice
- **THEN** the saved games are gone, the settings and configuration are kept, the save data names no game to continue, and play goes on in a new game

#### Scenario: No reset outside play
- **WHEN** engine.reset is called in a test
- **THEN** it returns false with "only in play" and the save data is unchanged
