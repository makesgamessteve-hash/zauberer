# Spec Delta: webxr-play

## Purpose

Playing in immersive VR: the controllers' poses, thumbsticks and buttons reach the game,
the carpet flies by reins or by head direction with comfort options against motion
sickness, weaves are cast by drawing gestures in the air with the hand that casts them,
and the HUD is a panel the player can read in the headset.

## ADDED Requirements

### Requirement: Controller data from the headset
Every XR frame SHALL report the left and right controllers to scripts in `xr.state().controllers`: whether each is tracked, its grip pose (else its target ray's) relative to the session origin, its thumbstick (x to the right, y pushed forward, each -1 .. 1), its trigger and its squeeze (0 .. 1), updated before the frame hooks like the head pose. Scenarios SHALL be able to emulate controllers with `xr.emulate_controller`.

#### Scenario: Controllers reach Lua
- **WHEN** the headset reports the right controller at (0.2, -0.3, -0.4) with its thumbstick half to the right and the trigger at 0.75
- **THEN** `xr.state().controllers.right` reports it tracked at that position with those values in the same frame, and an untracked controller at the session origin

### Requirement: Validated controller data
The engine SHALL accept frame data without controllers (both hands untracked) and SHALL reject controller data with a tracked flag other than 0 or 1, a non-finite number or a tracked controller without an orientation.

#### Scenario: Bad controller data
- **WHEN** the bridge reports a controller with a tracked flag of 0.5, a NaN in its pose or on its thumbstick, or a tracked controller with a zero orientation
- **THEN** the frame data is rejected

### Requirement: Flight steering in VR
During a session the carpet SHALL be steered by tilting a controller like reins (rolling turns, pitching climbs) or, as an option, by head direction (looking aside turns, looking up or down climbs), each relative to a neutral pose taken when the session starts and again when a grip is squeezed for a second.

#### Scenario: Steering with the reins
- **GIVEN** the rein hand's controller held tilted forward when the session starts
- **WHEN** it is rolled 30 degrees to the right, then pitched 20 degrees up
- **THEN** the flight input turns right, then climbs, and small tilts within the dead zone do nothing

#### Scenario: Steering with the head
- **GIVEN** head steering
- **WHEN** the player looks 40 degrees to the right, then up
- **THEN** the flight input turns right, then climbs

#### Scenario: Recentring
- **WHEN** the player squeezes a grip for a second while holding the reins tilted
- **THEN** that tilt becomes the neutral pose and no longer turns the carpet, while a shorter squeeze changes nothing

### Requirement: Thumbsticks and the seat
The left thumbstick SHALL control the throttle (y) and add turning (x), and the right thumbstick's y SHALL add climbing. The VR input SHALL add up with the other devices' flight input, and while riding the camera SHALL be the seat on the carpet.

#### Scenario: Thumbsticks
- **WHEN** the left thumbstick is pushed forward and half to the right and the right one three quarters back
- **THEN** the flight input has full throttle, half a right turn and three quarters of a descent, and deflections within the dead zone do nothing

#### Scenario: Flying in VR
- **GIVEN** an emulated headset and controllers in flight mode
- **WHEN** the left thumbstick is pushed forward, then the reins are rolled to the right and pitched up
- **THEN** the carpet speeds up, turns right and climbs

### Requirement: Gestures in the air
While a hand's trigger is held, its controller's path SHALL be recorded; when it is let go the path SHALL be projected onto the plane facing the player at the stroke's start (the head's right and up axes), in millimetres from its first point with y down, and recognised by the gesture recogniser. The trigger SHALL be that hand's cast button, and the recognised gesture SHALL shape the weave that hand is charging, or else become the form of the free weave, as a gesture drawn with the mouse does.

#### Scenario: Gestures in the air
- **WHEN** a circle and a line are drawn in the air facing the player, with the head turned and tilted in several directions
- **THEN** they are recognised as a circle and a line with an accuracy above 0.8, and a trigger pulled without moving is a tap

#### Scenario: Casting by drawing in the air
- **WHEN** the player holds the left trigger, draws a line in the air and lets go
- **THEN** the left hand's Fireball charges, the line reaches the magic as the left hand's gesture while it charges, and the Fireball is cast

### Requirement: Comfort options
The VR play SHALL take comfort options from its caller: snap turning (turning in 30 degree steps with a cooldown instead of smooth yaw, also for head steering), a reduced-speed mode (a lower top speed) and a horizon stabiliser (the seat stays level while the carpet banks and pitches). The defaults SHALL be reins, smooth turning, a vignette of 0.5, the stabiliser on and full speed.

#### Scenario: Snap turning
- **GIVEN** snap turning
- **WHEN** the reins are held rolled fully to the right for a second
- **THEN** the carpet turns by 30 degrees at once and again after the cooldown, never smoothly, and the vignette blinks with each snap

#### Scenario: Reduced speed
- **WHEN** the carpet flies at full throttle in reduced-speed mode
- **THEN** it reaches half its usual top speed

#### Scenario: Horizon stabiliser
- **WHEN** the carpet banks and pitches in a turn
- **THEN** the seat stays level with the stabiliser and banks and pitches with the carpet without it

### Requirement: Comfort vignette
A comfort vignette SHALL darken the edges of every view around its optical centre (`render.vignette(strength)`) with a strength that follows how fast the carpet turns and changes speed, up to the option's strength; a strength of 0 SHALL switch it off.

#### Scenario: Comfort vignette
- **WHEN** the carpet turns fast, then flies straight, then speeds up
- **THEN** the vignette rises to its strength, fades away and returns in part, and a strength of 0 darkens nothing

### Requirement: HUD in the headset
In VR the HUD lines SHALL be drawn as a wrapped panel low in front of the player, on a plane locked to the head at a fixed distance so both eyes see each line in the same direction, at the headset's resolution.

#### Scenario: HUD readable in the headset
- **GIVEN** an emulated headset in flight with the HUD on
- **WHEN** a frame is rendered
- **THEN** each canvas pixel lies in the same direction from the head in both eyes, the HUD appears below the centre of each eye's view, and nothing is drawn in the top corners
