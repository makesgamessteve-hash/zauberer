# game-flow Specification

## Purpose
TBD - created by archiving change add-title-and-game-flow. Update Purpose after archive.

## Requirements

### Requirement: Title screen
A player's game SHALL begin at a title over the waiting world with the game's name and version, offering Continue (New game at the first start), Create a world, Load game when games are saved, Settings, Controls, Credits and, except on the web, Quit; it SHALL be navigated like the menu on every device and left only by choosing.

#### Scenario: First start and later starts
- **WHEN** the game starts with no saves, Escape is pressed and New game chosen, and the title is shown again after two games were saved
- **THEN** the first title shows the version and New game but not Continue or Load game, the world waits, Escape does not leave it, New game begins play, and the later title offers Continue and Load game

### Requirement: Choosing a saved game
Load game SHALL list the saved games newest first with their game day; a chosen game SHALL be played (the one being played saved first) or deleted, deleting only after the question is answered yes and never the game being played; while the land under the heir is still being made the HUD SHALL say how far it has got.

#### Scenario: Loading and deleting
- **WHEN** two games are saved, the older is played from the list, the newer deleted with No and then Yes, and the game being played is chosen for deleting
- **THEN** the list shows the newer first with days, the older continues where it was saved, No keeps the newer and Yes deletes it, and the game being played is kept with a reason
