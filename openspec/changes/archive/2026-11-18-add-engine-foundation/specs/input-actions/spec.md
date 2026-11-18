# Spec Delta: input-actions

## Purpose

Defines device-independent input: scripts work with named actions while players use
keyboard and mouse, gamepads or touch, with the right control scheme selected automatically.

## ADDED Requirements

### Requirement: Named actions
Scripts SHALL define named actions of type button, 1D axis or 2D axis, and SHALL read their per-tick state (pressed, released, held, value). The state SHALL be sampled once before each simulation tick and stay constant during that tick.

#### Scenario: Button held across ticks
- **GIVEN** an action `cast_left` bound to the left mouse button
- **WHEN** the button is pressed for 10 ticks and released
- **THEN** `pressed` is true on the first tick, `held` on ticks 1 to 10 and `released` on the tick after

### Requirement: Default bindings from packages
Default bindings for keyboard and mouse, gamepad and touch SHALL be defined as package data. Later packages may override them, using overlay rules.

#### Scenario: Mod changes a default binding
- **GIVEN** a mod that binds `jump` to a different key
- **WHEN** no user override exists
- **THEN** the mod's binding is active

### Requirement: User rebinding and persistence
Players SHALL be able to rebind any action. User bindings SHALL be stored in the user configuration and applied at the next start. Unknown actions or invalid keys in the stored configuration SHALL be ignored with a warning.

#### Scenario: Rebinding survives restart
- **WHEN** the player rebinds `jump` and restarts the game
- **THEN** `jump` uses the new binding

#### Scenario: Stale configuration entry
- **GIVEN** the user configuration references an action that no package defines
- **WHEN** the engine starts
- **THEN** the entry is ignored with one warning and all other bindings apply

### Requirement: Gamepad support
The engine SHALL support standard-layout gamepads with hot-plugging: connecting a gamepad at runtime makes its bindings active, and disconnecting it releases all actions it was holding.

#### Scenario: Unplug while holding
- **GIVEN** an action is held by a gamepad button
- **WHEN** the gamepad is disconnected
- **THEN** the action reports `released` on the next tick

### Requirement: Automatic touch mode
On the web the engine SHALL start in touch mode when the device's primary pointer is coarse and touch points exist. It SHALL switch to touch mode on the first real touch, and back to desktop mode on physical mouse or keyboard input. Mouse events synthesised from touches SHALL be ignored.

#### Scenario: Phone starts in touch mode
- **WHEN** the web build starts under mobile device emulation with a coarse pointer
- **THEN** `engine.info().input_mode` is `touch` and scripts receive an input-mode event

#### Scenario: Keyboard on a tablet
- **GIVEN** touch mode is active
- **WHEN** a physical key is pressed
- **THEN** the input mode switches to `desktop`

#### Scenario: Synthesised mouse events
- **GIVEN** touch mode is active
- **WHEN** a touch produces browser-synthesised mouse events
- **THEN** the input mode stays `touch`

### Requirement: Virtual touch controls
The engine SHALL provide virtual stick and virtual button primitives with screen regions defined by scripts. Touches inside a region SHALL drive the bound actions, and each touch SHALL stay owned by the control it started on.

#### Scenario: Virtual stick drives an axis
- **GIVEN** a virtual stick bound to `move`
- **WHEN** a touch starts on the stick and drags fully to the right
- **THEN** `move` reports approximately (1, 0)

### Requirement: Input recording and replay
With `--record <file>` the engine SHALL write every tick's action snapshot to a file. With `--replay <file>` it SHALL feed those snapshots back instead of live devices.

#### Scenario: Replay reproduces actions
- **WHEN** a session is recorded and then replayed
- **THEN** scripts observe identical action states on every tick in both runs
