# accessibility Specification

## Purpose
Accessibility: settings kept with the player's data, the menu, remapping, toggle instead of hold, gesture assist, colour glyphs and colour-vision palettes, reduced flashing, field of view, sound captions and story markers.

## Requirements

### Requirement: Settings are kept
The settings (language, text size, high contrast, colour vision, colour glyphs, reduced flashing, captions, toggle instead of hold, gesture assist, look curve and speed, field of view, story markers, survival, and the VR comfort options) SHALL be kept with the player's data, applied at once, and invalid values refused.

#### Scenario: Settings are kept
- **WHEN** settings are changed, refused and loaded again
- **THEN** valid ones are kept and applied, invalid ones refused or dropped

### Requirement: The menu
The menu SHALL offer resume, settings, controls, packages, credits and save and quit; up and down choose, left and right change a setting, confirm opens or does it, Escape goes back.

#### Scenario: The menu
- **WHEN** the menu is used to change the language, look at packages and credits, resume and quit
- **THEN** each does what it says

### Requirement: Escape opens the menu in play
In play Escape SHALL open the menu; while it is open the heir's own actions SHALL be idle; alone the world SHALL stand still, in a co-op session it goes on; Escape SHALL close it.

#### Scenario: Escape opens the menu in play
- **WHEN** Escape is pressed in flight alone, a cast is tried and Escape pressed again; and the menu is opened in a co-op session
- **THEN** the menu opens, no cast happens, no game time passes alone, the menu closes and time goes on; in company time passes with the menu open

### Requirement: Remapping controls
A control SHALL be remapped by choosing it and pressing a key or button; the new binding SHALL replace those of the same device (keyboard and mouse, gamepad, touch, VR) and keep the others; Escape SHALL leave it as it was.

#### Scenario: Remapping controls
- **WHEN** a key binding and a gamepad binding are remapped, and a remapping is abandoned
- **THEN** only the same device's binding is replaced, and the abandoned one is unchanged

### Requirement: Remapping takes the next key or button
The engine SHALL report the binding of the last key, mouse button or gamepad button pressed (not released) once.

#### Scenario: Remapping takes the next key or button
- **WHEN** a key, a mouse button and a gamepad button are pressed
- **THEN** each is reported once as key:x, mouse:right and gamepad:a

### Requirement: Toggle instead of hold
With toggle instead of hold, a press SHALL switch a held action (casting, drawing, channeling, boost) on and the next off, the same however often it is read in a tick; the look curve and speed SHALL shape the look axes; while the menu is open the game's actions read as idle.

#### Scenario: Toggle instead of hold
- **WHEN** channel is pressed, let go and pressed again with toggling on, and look is read with a curve
- **THEN** it stays on until the second press and look is shaped

### Requirement: Gesture assist
Gesture assist SHALL be off, tolerant (kinder to sloppy shapes) or snap (any recognised form counts as drawn well enough).

#### Scenario: Gesture assist
- **WHEN** gestures are recognised with each setting
- **THEN** tolerance follows the setting and snap raises the accuracy of recognised forms only

### Requirement: Colour is never the only cue
Every mana colour SHALL have a glyph shown beside its name (unless turned off): fire *, storm ~, light ^, life +, water =, earth #, void @, decay %.

#### Scenario: Colour is never the only cue
- **WHEN** colours are named with glyphs on and off
- **THEN** the glyph is beside the name only when on

### Requirement: Colour vision modes
The image SHALL be corrected for deuteranopia, protanopia or tritanopia when chosen, and left as it is with full colour vision.

#### Scenario: Colour vision modes
- **WHEN** each mode is applied to known colours
- **THEN** full leaves them; the others shift confusable colours apart

### Requirement: Calmer light and camera
Reduced flashing SHALL dim lightning and other flashes to a fifth; the field of view SHALL be the player's.

#### Scenario: Calmer light and camera
- **WHEN** reduced flashing and a field of view are set
- **THEN** flashes are a fifth as bright and the camera uses the field of view

### Requirement: Sound captions
Captions SHALL say what can be heard and from which side (ahead, behind, left, right): thunder, fire close by and calling creatures; each stays a few seconds; none when captions are off.

#### Scenario: Sound captions
- **WHEN** lightning strikes, fire burns to the right and a wolf is behind
- **THEN** the captions say so, and none when off

### Requirement: Story markers
Story markers SHALL show the way to the next of the Circle only when the player asks for them.

#### Scenario: Story markers
- **WHEN** markers are off and on
- **THEN** a marker line appears only when on

### Requirement: Losing focus
When the window loses focus, is hidden or minimised, the app goes to the background or the web page is hidden, the game SHALL open the menu in play and mute its sound, and on the web save the game; with focus back the sound SHALL return and the menu stay open.

#### Scenario: Focus lost and back
- **WHEN** focus is lost in flight and comes back, and the web page is hidden and shown
- **THEN** the menu opens, the sound is muted and the world stands still; with focus back the sound returns and the menu is still open; the page's visibility reaches the game
