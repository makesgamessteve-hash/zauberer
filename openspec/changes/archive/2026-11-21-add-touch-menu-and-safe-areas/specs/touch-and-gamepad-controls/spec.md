## MODIFIED Requirements

### Requirement: Touch layout
On touch devices the flight SHALL add, once, a steer stick, a climb stick, a boost, a chime, a menu, a credits and an OK button, each drawn with its label inside the screen; the menu button SHALL open the menu and go back in it, the steer stick choose and OK confirm, and the menu SHALL say so; the web page SHALL keep the game inside the device's safe area.

#### Scenario: Touch layout
- **WHEN** the game starts on a touch-first device and the menu is opened with the menu button
- **THEN** the flight's on-screen controls are set up once, each labelled inside the screen, the menu and OK buttons are bound to the menu's actions, and the menu's help names the stick, OK and Menu

#### Scenario: Opening the menu by touch
- **WHEN** the web game runs on an emulated phone and the menu button is tapped
- **THEN** the game lies inside the page and the menu opens
