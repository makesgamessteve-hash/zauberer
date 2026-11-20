## ADDED Requirements

### Requirement: Starting the game over
The reset key (F9 by default) SHALL ask first: pressed once, the HUD says to press it again; pressed again within five seconds, the game SHALL start over as at its first start. Starting over SHALL delete every saved game, keep only the player's settings in the save data and the configuration (bindings, packages) as it is, save nothing of the game left, and make the world, the simulation and the scripts anew without leaving the window. It SHALL be refused, with the reason shown, in a co-op session and in tests and scenarios.

#### Scenario: The reset key asks first
- **WHEN** the reset key is pressed once, then again within five seconds, then once more after the time ran out
- **THEN** the first press asks, the second starts over (or says why it cannot), and the third asks again

#### Scenario: Reset to the first start
- **WHEN** a game with a saved world, an old world file and settings is played and the reset key pressed twice
- **THEN** the saved games are gone, the settings and configuration are kept, the save data names no game to continue, and play goes on in a new game

#### Scenario: No reset outside play
- **WHEN** engine.reset is called in a test
- **THEN** it returns false with "only in play" and the save data is unchanged
