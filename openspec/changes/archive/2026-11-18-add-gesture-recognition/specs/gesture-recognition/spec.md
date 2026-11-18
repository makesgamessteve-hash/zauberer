## ADDED Requirements

### Requirement: The eight forms
The recogniser SHALL tell the eight forms of weaves (tap, line, circle, spiral, triangle, square, zigzag, wave) from a stroke of 2D points, reporting the best form with an accuracy from 0 to 1, the second-best form, and the stroke's size, centre and direction.

#### Scenario: The eight forms
- **WHEN** each form's template is drawn on screen
- **THEN** each is recognised as itself with an accuracy above 0.95, and the second-best form is another one that scores lower

### Requirement: Tolerant matching
Strokes SHALL be recognised whatever their orientation, size, drawing direction or number of points, mirrored or not, closed forms from any point on their outline, and with a shaky hand.

#### Scenario: Any orientation, either way round
- **WHEN** each form is drawn turned, backwards, mirrored, larger with more points, smaller with fewer points and with jitter, and a circle and a square are started elsewhere on their outline
- **THEN** each is recognised as its form, above 0.9 accuracy when drawn cleanly and above 0.7 with jitter

### Requirement: Taps, scribbles and assist
Strokes shorter than the tap size SHALL be taps, strokes like no form SHALL be refused, and gesture assist SHALL judge sloppy shapes more kindly.

#### Scenario: Taps, scribbles and assist
- **WHEN** a short stroke, a stroke shorter than a larger tap size, a stroke that does not move, a random scribble and a sloppy triangle with and without assist are recognised, and nothing is given
- **THEN** the short and still strokes are taps, the flick is a line once the tap size is small, the scribble is no form, the triangle scores higher with assist, and empty input is no form

### Requirement: Gestures in the game
Scripts SHALL list the forms, recognise strokes and read the forms' templates, and a stroke SHALL be traced from pointer or stick movement while the draw button is held and recognised on release, with its trail kept for the HUD.

#### Scenario: Gestures from Lua
- **WHEN** a circle is traced with mouse motion and a line with a stick while the button is held, a tap is made, and the gesture functions are called with good and bad arguments
- **THEN** the circle and the line are recognised on release, the trail fits the HUD's radius, the tap is a tap, and odd point lists, unknown forms and bad counts are refused
