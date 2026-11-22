## ADDED Requirements

### Requirement: The UI grid
The UI SHALL be drawn after the world into the internal framebuffer (as tall as the pixel resolution setting, as wide as the window's aspect gives) before it is scaled to the window, untouched by the world's palette quantisation and dithering, and SHALL keep inside the platform's safe area.

#### Scenario: Window sizes
- **WHEN** the window is 1920×1080 and 2560×1080 at a pixel resolution of 360, and a phone reports insets for a notch
- **THEN** the grid is 640×360 and 853×360, a UI colour comes out exactly as given, and the safe box lies inside the insets

### Requirement: Drawing rectangles, images and frames
Scripts SHALL be able to draw filled rectangles, images and parts of images (tinted, with alpha), nine-slice frames whose edges and middle are tiled in whole repeats, and text with a shadow, all in grid pixels; calls SHALL be batched by texture.

#### Scenario: A framed panel
- **WHEN** a nine-slice frame is drawn at 100×40 and at 9×9 with 6-pixel corners, a part of an image is drawn tinted at twice its size, and twenty rectangles are drawn in a row
- **THEN** the 100×40 frame has whole corners and edges tiled without stretching, the 9×9 frame draws its corners only, the part comes out twice its size in the tint, and the twenty rectangles come in one batch

### Requirement: Clipping
Scripts SHALL be able to clip UI drawing to a rectangle and release it, nested clips intersecting.

#### Scenario: A scrolled list
- **WHEN** a list of twenty rows is drawn inside a clip of five rows, inside a clip that cuts its right half
- **THEN** only what lies inside both clips is drawn

### Requirement: Textures made by scripts
Scripts SHALL be able to make an RGBA texture of up to 1024×1024 pixels from a string of bytes, replace a region of it and free it; a wrong size SHALL be refused with an error.

#### Scenario: A painted texture
- **WHEN** a 64×64 texture is made, a 4×4 region replaced, it is drawn and freed, and a texture of 2048×2048 or with too few bytes is asked for
- **THEN** the texture is drawn, the region is replaced, and the bad ones are refused

### Requirement: Pointing
Scripts SHALL be able to read the pointer (the mouse, or the last touch) in the grid's pixels with whether it is held, pressed or released this frame and the wheel; a layout kit SHALL place boxes at nine anchors of the safe area and say when a box is pointed at and clicked or tapped; a touch beginning inside a box the UI claims SHALL not move the flight controls under it.

#### Scenario: Anchors and a tap
- **WHEN** a 50×20 box is anchored bottom-right with a 4-pixel offset and claimed, and a tap lands on it over the touch climb stick
- **THEN** the box sits 4 pixels inside the safe area's corner, the box reports the tap, and the climb stick does not move
