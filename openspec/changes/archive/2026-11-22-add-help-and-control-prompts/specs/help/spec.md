## ADDED Requirements

### Requirement: Control prompts for the device in hand
The engine SHALL report the device used last (keyboard and mouse, gamepad or touch; a stick's drift does not count), and prompts SHALL name the control of that device for the action they ask for, changing when the player switches device.

#### Scenario: Switching device
- **WHEN** a stick drifts, is pushed over, a key, a gamepad button, a mouse button and a touch follow, and a prompt is filled for the keyboard and for a gamepad
- **THEN** the device follows each but the drift, and the prompt names the key, then the gamepad control

### Requirement: First-time hints
When the heir can first fly, open the menu, step off, get on, use something, cast, use the bag, or is tired or hungry, a hint SHALL say how with its controls, once each for the player and for six seconds; the hints SHALL be kept with the player's data, not shown when turned off, and shown again after the help page asks for it.

#### Scenario: A hint once
- **WHEN** the heir flies, six seconds pass, the hints are loaded again, turned off, reset and turned on
- **THEN** the flying hint shows, then the menu hint, neither again; they are kept; none shows while off, and they show again after the reset

### Requirement: The help pages
The title and the menu SHALL offer help pages: the controls of the device in hand, flying, walking, casting with the eight forms, survival, the bag and tent, and the story so far, all translated.

#### Scenario: Reading help
- **WHEN** each help page is opened in English and in German
- **THEN** every page has its lines with each control named, the controls page lists the keys, and nothing but names and numbers lacks German
