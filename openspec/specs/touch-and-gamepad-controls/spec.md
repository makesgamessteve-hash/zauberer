# touch-and-gamepad-controls Specification

## Purpose
Makes the flight controllable with keyboard and mouse, gamepads and touch screens
through the same input actions, with shaped responses and a touch layout for phones.

## Requirements

### Requirement: Flight actions on every device
The flight SHALL read only the actions steer, climb, climb_touch, boost and look, and the default bindings SHALL map keyboard and mouse, gamepad and touch controls to them.

#### Scenario: Same actions on every device
- **WHEN** the steer, climb, climb_touch, boost and look actions are set as any device would set them
- **THEN** the carpet responds the same way

### Requirement: Gamepad axes
Gamepad axis bindings SHALL accept a leading minus sign to reverse the axis, and single axes SHALL read zero inside the stick dead zone.

#### Scenario: Inverted gamepad axis
- **WHEN** climb is bound to "gamepad:-right_y" and the right stick is pushed up to -0.8, then rests at 0.1
- **THEN** climb reads +0.8, then 0

### Requirement: Response curve
Stick and key values SHALL be shaped by a sign-preserving power curve with exponent 1.6, clamped to -1..1.

#### Scenario: Response curve
- **WHEN** inputs of 0, 0.5, -0.5 and 2 are shaped
- **THEN** the results are 0, 0.5^1.6, -(0.5^1.6) and 1

### Requirement: Touch layout
On touch devices the flight SHALL add, once, a steer stick, a climb stick, a boost, a chime, a menu, a credits and an OK button, each drawn with its label inside the screen; the menu button SHALL open the menu and go back in it, the steer stick choose and OK confirm, and the menu SHALL say so; the web page SHALL keep the game inside the device's safe area.

#### Scenario: Touch layout
- **WHEN** the game starts on a touch-first device and the menu is opened with the menu button
- **THEN** the flight's on-screen controls are set up once, each labelled inside the screen, the menu and OK buttons are bound to the menu's actions, and the menu's help names the stick, OK and Menu

#### Scenario: Opening the menu by touch
- **WHEN** the web game runs on an emulated phone and the menu button is tapped
- **THEN** the game lies inside the page and the menu opens
