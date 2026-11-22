## ADDED Requirements

### Requirement: The UI grid
The UI SHALL be drawn after the world on a grid of whole-number-scaled pixels derived from the internal resolution (scale = the largest whole number that fits the window, at least 1), untouched by the world's palette quantisation and dithering, and SHALL keep inside the platform's safe area.

#### Scenario: Window sizes
- **WHEN** the window is 1920×1080, 2560×1440 and 1366×768 at an internal resolution of 640×360, and a phone reports insets for a notch
- **THEN** the grid is 640×360 at scale 3, 640×360 at scale 4 and 683×384 at scale 2, a UI colour comes out exactly as given, and the safe box lies inside the insets

### Requirement: Drawing rectangles, images and frames
Scripts SHALL be able to draw filled and outlined rectangles, images and atlas cells (tinted, with alpha, at a whole-number scale), nine-slice frames whose edges and middle are tiled in whole repeats, and text with a shadow or outline, all in grid pixels; calls SHALL be batched by texture and clip rectangle.

#### Scenario: A framed panel
- **WHEN** a nine-slice frame is drawn at 100×40 and at 9×9 with 6-pixel corners, an atlas cell is drawn tinted at scale 2, and twenty rectangles and three texts are drawn with one texture
- **THEN** the 100×40 frame has whole corners and edges tiled without stretching, the 9×9 frame draws its corners only, the cell is twice its size in the tint, and the frame's calls come in one batch

### Requirement: Clipping
Scripts SHALL be able to clip UI drawing to a rectangle and release it, nested clips intersecting.

#### Scenario: A scrolled list
- **WHEN** a list of twenty rows is drawn inside a clip of five rows, inside a clip that cuts its right half
- **THEN** only the rows inside both clips are drawn

### Requirement: Textures made by scripts
Scripts SHALL be able to make an RGBA texture of up to 1024×1024 pixels from a string of bytes, replace a region of it and free it; a wrong size SHALL be refused with an error.

#### Scenario: A painted texture
- **WHEN** a 64×64 texture is made, a 4×4 region replaced, it is drawn, freed, and a texture of 2048×2048 or with too few bytes is asked for
- **THEN** the drawn pixels are the ones given, the region is replaced, and the bad ones are refused

### Requirement: Layout and pointing
A layout kit SHALL place boxes at nine anchors of the safe area and stack them, and SHALL say when the mouse or a touch is over a box and when it was clicked or tapped; a tapped box SHALL take the touch from the flight controls under it.

#### Scenario: Anchors and a tap
- **WHEN** a 50×20 box is anchored bottom-right with a 4-pixel offset, and a tap lands on it over the touch climb stick
- **THEN** the box sits 4 pixels inside the safe area's corner, the box reports the tap, and the climb stick does not move
