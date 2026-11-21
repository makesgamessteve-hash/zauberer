# game-loop Specification

## Purpose
Defines how simulation time advances: a fixed-step, deterministic simulation that is
independent of the rendering frame rate and can run headless for tools and automated tests.

## Requirements

### Requirement: Fixed simulation step
The simulation SHALL advance in fixed ticks of 1/60 s, independent of the rendering frame rate, so that one second of real time yields 60 ticks when the machine keeps up.

#### Scenario: Low frame rate
- **GIVEN** rendering runs at 30 frames per second
- **WHEN** one second of real time passes
- **THEN** 60 simulation ticks have run

#### Scenario: High frame rate
- **GIVEN** rendering runs at 144 frames per second
- **WHEN** one second of real time passes
- **THEN** 60 simulation ticks have run

### Requirement: Bounded catch-up
When a frame takes too long, the loop SHALL run at most 8 ticks before rendering again, discarding the remaining backlog and logging the slowdown, so that a stall never leads to an ever-growing backlog.

#### Scenario: Two-second stall
- **WHEN** the process is suspended for two seconds and resumes
- **THEN** the next frame runs at most 8 ticks
- **AND** a slowdown warning is logged once

### Requirement: Interpolated rendering
Each rendered frame SHALL receive an interpolation factor in the range [0, 1) describing its position between the last two simulation states.

#### Scenario: Frame between ticks
- **WHEN** a frame is rendered halfway between two ticks
- **THEN** the interpolation factor passed to rendering and to frame hooks is 0.5 within 0.01

### Requirement: Headless execution
With `--headless` the engine SHALL run without creating a window, graphics context or audio device, stepping the simulation as fast as possible. `--ticks N` SHALL stop after exactly N ticks.

#### Scenario: Run a fixed number of ticks
- **WHEN** the engine is started with `--headless --ticks 600`
- **THEN** exactly 600 ticks run and the process exits with code 0
- **AND** no window, graphics context or audio device is created

### Requirement: Deterministic simulation
Given the same seed, packages and per-tick input stream, the simulation SHALL produce an identical state hash after every tick, across runs and across all supported platforms.

#### Scenario: Same inputs, same hash
- **WHEN** a recorded input stream is replayed twice with the same seed
- **THEN** the state hash after the last tick is identical in both runs

#### Scenario: Cross-platform hash
- **WHEN** the same replay runs on Linux, Windows, macOS and the web build
- **THEN** all platforms report the same final state hash

### Requirement: Pause and time scale
The engine SHALL support pausing the simulation while rendering and UI continue, and in development builds SHALL support a time scale between 0.1 and 10.

#### Scenario: Paused game keeps rendering
- **WHEN** a script pauses the simulation
- **THEN** no simulation ticks run while frames and UI hooks continue to be called

### Requirement: Holding the world
Scripts SHALL be able to hold the world still: while it is held, no game time SHALL pass and the active tier (fire, water, air, creatures) SHALL not advance, while input is sampled and the tick and frame hooks go on.

#### Scenario: The world held and let go
- **WHEN** the world is held for ten ticks and let go
- **THEN** game time does not change while it is held, the scripts' tick hook keeps running (the menu answers), and time goes on once it is let go
