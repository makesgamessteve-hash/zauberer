## ADDED Requirements

### Requirement: Sound, picture and control settings
The settings SHALL offer volume, music and sounds (0 to 100 %), mute, V-sync, a frame cap (none, 30, 60, 120), the pixel resolution (240 to 720 lines), brightness (0.75 to 1.5) and invert look; each SHALL apply at once, be kept, and invalid values be refused; the web leaves V-sync and the frame cap to the browser.

#### Scenario: Changing and keeping settings
- **WHEN** music is set to 0, the volume to 60 % and muted and unmuted, the resolution set to 480 lines, brightness to 1.25, V-sync off and a cap of 60, invert look on, and the settings loaded again
- **THEN** music is silent and sounds play, the master follows mute, the scene is 480 lines high and brighter, looking up turns the view down, invalid values are refused, and all is kept

### Requirement: Text size and high contrast
Text size SHALL scale the HUD and menus by 1, 2 or 3 times, every line wrapped at spaces to the screen (a word longer than the line cut); high contrast SHALL outline the text in black.

#### Scenario: Large text
- **WHEN** text size is 2 and a line far longer than the screen is wrapped and drawn
- **THEN** it comes out in pieces none longer than the screen, a short line stays whole, and an overlong word is cut

### Requirement: Back to defaults
The settings SHALL offer reset to defaults after a question answered yes, keeping the language.

#### Scenario: Reset
- **WHEN** several settings are changed, the question is answered No and then Yes
- **THEN** No keeps them; Yes sets each back to its default (the resolution 360 lines again) and the language is unchanged
