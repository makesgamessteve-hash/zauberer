## MODIFIED Requirements

### Requirement: Title screen
A player's game SHALL begin at a title over the waiting world with the game's name in the carpet's colours over a woven band and the version in a corner, offering in a framed panel Continue (New game at the first start), Create a world, Load game when games are saved, Settings, Controls, Credits and, except on the web, Quit, with beside it the name and day of the save Continue would load; it SHALL be navigated like the book on every device, the mouse and touch included, and left only by choosing.

#### Scenario: First start and later starts
- **WHEN** the game starts with no saves, Escape is pressed and New game chosen, and the title is shown again after two games were saved
- **THEN** the first title shows the version and New game but not Continue, Load game or a save's summary, the world waits, Escape does not leave it, New game begins play, and the later title offers Continue and Load game with the newer save's summary beside them
