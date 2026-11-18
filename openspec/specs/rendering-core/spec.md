# rendering-core Specification

## Purpose
Defines the minimal renderer that later milestones build on: models, sprites and text drawn
at a low internal resolution for the retro look, through a view list that already supports stereo.

## Requirements

### Requirement: Internal resolution with nearest-neighbour upscale
The scene SHALL be rendered at a configurable internal height (default 360 pixels, width from the window's aspect ratio) and upscaled to the window with nearest-neighbour filtering, using integer scale factors when the window allows.

#### Scenario: Full HD window
- **WHEN** the window's drawable size is 1920 × 1080 with default settings
- **THEN** the scene renders at 640 × 360 and is upscaled by a factor of 3 without filtering

#### Scenario: Non-integer window size
- **WHEN** the drawable size is 1000 × 700
- **THEN** the scene renders at an internal height of 360 and fills the window, with any remaining border filled with the clear color

### Requirement: Model rendering
The engine SHALL load static meshes from glTF 2.0 files (`.glb` and `.gltf`) in packages, including node transforms, vertex colors and base color textures, and render them with per-vertex lighting from one directional light and an ambient term.

#### Scenario: Render the carpet model
- **WHEN** the base package's carpet model is drawn in the reference scene
- **THEN** the rendered image matches the golden image within the configured tolerance

#### Scenario: Corrupt model
- **WHEN** a model file cannot be parsed
- **THEN** a placeholder cube is rendered and an error naming the file is logged

### Requirement: Sprite rendering
The engine SHALL render camera-facing billboard sprites from PNG sprite sheets described by package data (frames, pivot, frame rate), with alpha-tested transparency and frame animation.

#### Scenario: Animated sprite
- **GIVEN** a sprite with 4 frames at 8 frames per second
- **WHEN** 0.5 s of simulation time passes
- **THEN** the sprite has advanced exactly 4 frames

### Requirement: Text rendering
The engine SHALL render UTF-8 text with bitmap fonts from packages. Characters missing from the font SHALL render as a visible replacement glyph.

#### Scenario: Missing glyph
- **WHEN** a string contains a character that the font lacks
- **THEN** a replacement glyph is drawn in its place and the rest of the string renders normally

### Requirement: Multi-view rendering
Each frame SHALL render a list of views with their own camera matrices and viewports. The default is one view. A development flag SHALL force two side-by-side views to exercise the stereo path that WebXR will use.

#### Scenario: Forced stereo
- **WHEN** the engine runs with `--debug-views 2`
- **THEN** the scene is rendered twice, side by side, with the configured eye offset
- **AND** the image matches the stereo golden image within tolerance

### Requirement: Consistent output across graphics backends
Rendering SHALL use only features common to OpenGL 3.3 core and OpenGL ES 3.0. Reference scenes SHALL match the same golden images within tolerance on desktop OpenGL and on WebGL 2.

#### Scenario: Desktop and web agree
- **WHEN** the reference scene is rendered by the desktop build on a software OpenGL driver and by the web build in a headless browser
- **THEN** both images match the same golden image within tolerance

### Requirement: Rendering-free headless mode
In headless mode the renderer SHALL build draw lists without issuing graphics API calls, so that scene logic can be tested without a GPU.

#### Scenario: Draw list inspection
- **WHEN** a headless scenario places three models in view and one behind the camera
- **THEN** the frame's draw list contains exactly three model draws
