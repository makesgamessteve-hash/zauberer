# webxr-session Specification

## Purpose
Immersive VR in the browser: the web build enters a WebXR `immersive-vr` session through
its own JavaScript bridge, renders the scene once per headset view with the retro look,
reports the head pose to scripts relative to the session origin (the seat on the carpet)
and turns controller select and squeeze into input actions. Desktop builds compile the
same code with the path inactive, and scenarios can emulate a headset.

## Requirements

### Requirement: Entering and leaving VR
The web page SHALL show an Enter VR button only when the browser supports `immersive-vr` sessions. Pressing it SHALL start an `immersive-vr` session with a `local` reference space and a WebGL layer on the engine's context; the session's frames SHALL then drive the engine. When the session ends, the page's normal frame loop SHALL render to the canvas again.

#### Scenario: No button without support
- **WHEN** the page loads in a browser without an `immersive-vr` device
- **THEN** no Enter VR button is shown and the engine reports XR as unavailable

#### Scenario: Enter and leave
- **GIVEN** a browser whose WebXR device supports `immersive-vr` (an emulated device in tests)
- **WHEN** the player presses Enter VR and the session later ends
- **THEN** the engine renders XR frames while the session runs, renders one view to the canvas again afterwards, and reports no script errors

### Requirement: Stereo views from the headset
During a session every frame SHALL be rendered once per headset view: with the view's matrix applied relative to the camera that scripts set (the session origin), with the view's field of view and the camera's near and far planes, at the internal resolution, upscaled with nearest-neighbour filtering into the view's viewport of the XR layer's framebuffer.

#### Scenario: Views from the headset
- **GIVEN** a camera and two headset views with their own view matrices, asymmetric projections and viewports
- **WHEN** a frame is rendered
- **THEN** each view's matrix is the headset's view matrix times the camera's, its projection keeps the headset's field of view with the camera's near and far planes, the internal framebuffer is split between the views in proportion to their viewports, and objects are culled per view

#### Scenario: Two views in the XR layer
- **GIVEN** an emulated WebXR device whose two views look in different directions, with viewports that leave a border of the layer's framebuffer free
- **WHEN** the session runs
- **THEN** each viewport of the layer's framebuffer shows the sky as seen by its own view, and the border stays untouched

#### Scenario: Emulated headset on the desktop
- **WHEN** a scenario emulates a headset in a 640 × 360 window
- **THEN** two views render side by side into the window and match the XR stereo golden image within tolerance, and ending the emulation returns to one view from the camera

### Requirement: Head tracking for scripts
`xr.state()` SHALL report whether a session is available and running, the number of views, the number of XR frames rendered in the session and the viewer (head) pose relative to the session origin, updated before the frame hooks of every XR frame. Scripts SHALL receive an `xr_session` event when a session starts or ends.

#### Scenario: Head pose reaches Lua
- **WHEN** the headset reports the viewer at position (0.1, 0.2, -0.3) turned 30° to the left
- **THEN** `xr.state().head` reports that position and orientation within 1e-5 during the same frame

### Requirement: Controller buttons as input actions
Select and squeeze of XR input sources SHALL become input events that actions bind as `xr:select` and `xr:squeeze` (any hand) or `xr:left_select`, `xr:right_select`, `xr:left_squeeze` and `xr:right_squeeze`. They SHALL behave like other buttons, including in recordings and replays.

#### Scenario: Select presses an action
- **WHEN** the right controller's select starts and ends between two ticks
- **THEN** actions bound to `xr:select` and `xr:right_select` are pressed in the next tick and an action bound to `xr:left_select` is not

### Requirement: Validated frame data
The engine SHALL reject frame data from the bridge with an unsupported view count, a non-positive framebuffer or viewport size, a viewport outside the framebuffer or a non-finite number, and SHALL render nothing from rejected data.

#### Scenario: Bad frame data
- **WHEN** the bridge reports three views, a viewport outside the framebuffer or a NaN in a matrix
- **THEN** the frame data is rejected

### Requirement: Inactive on the desktop
Desktop and headless builds SHALL compile the same XR code with the session path inactive: `xr.state()` reports no available or running session, and frames render from the camera.

#### Scenario: No session on the desktop
- **WHEN** a headless run queries `xr.state()` without emulating a headset
- **THEN** `available` and `active` are false, `views` is 0 and frames have one view from the camera
