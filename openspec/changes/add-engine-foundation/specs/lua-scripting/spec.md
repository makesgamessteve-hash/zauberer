# Spec Delta: lua-scripting

## Purpose

Defines the Lua 5.4 game layer: how package scripts are started and called, what they may
and may not do, and how script errors are contained so that the game stays modifiable and robust.

## ADDED Requirements

### Requirement: Package entry scripts
After packages load, the engine SHALL execute each package's `scripts/main.lua`, if present, in package load order. `require` SHALL resolve module names through the package file system.

#### Scenario: Entry scripts run in order
- **GIVEN** `base` and a mod loaded after it both have `scripts/main.lua`
- **WHEN** the engine starts
- **THEN** the base script runs before the mod script

#### Scenario: Require a package module
- **WHEN** a script calls `require("ui.menu")`
- **THEN** `scripts/ui/menu.lua` is loaded from the packages using overlay lookup

### Requirement: Lifecycle hooks
Scripts SHALL be able to register `on_init`, `on_tick`, `on_frame`, `on_event`, `on_shutdown` and, in development builds, `on_reload`. The engine SHALL call `on_tick` once per simulation tick and `on_frame` once per rendered frame, in package load order.

#### Scenario: Tick hook runs per tick
- **GIVEN** a script registers `on_tick`
- **WHEN** the engine runs headless for 120 ticks
- **THEN** the hook is called exactly 120 times with the fixed tick duration

### Requirement: Sandbox
Scripts SHALL NOT have access to `os.execute`, `io`, `dofile`, `loadfile`, `package.loadlib`, binary chunk loading or real-time clocks. File access SHALL go through the read-only package file system and the save API only.

#### Scenario: Forbidden call
- **WHEN** a script calls `os.execute("ls")` or `io.open("x", "w")`
- **THEN** the call fails with an error stating that the function is unavailable

#### Scenario: Binary chunk rejected
- **WHEN** a script calls `load` with precompiled bytecode
- **THEN** loading fails and no code runs

### Requirement: Error isolation
A runtime error in a script hook SHALL be logged with package id, file, line and traceback. That hook SHALL be disabled for that package and the engine SHALL keep running. Development builds SHALL also show the error on screen.

#### Scenario: Failing tick hook
- **GIVEN** a mod's `on_tick` raises an error
- **WHEN** the tick runs
- **THEN** the error is logged with traceback, the mod's `on_tick` is no longer called
- **AND** other packages' hooks and the simulation continue

### Requirement: Deterministic script environment
Scripts SHALL get randomness only from engine-seeded deterministic streams. `math.random` SHALL be replaced by a deterministic generator seeded from the world seed. The only time source SHALL be simulation time.

#### Scenario: Repeatable randomness
- **WHEN** a script draws 1,000 values from `math.random` in two runs with the same seed
- **THEN** both runs yield the same sequence

### Requirement: Engine information for scripts
Scripts SHALL be able to query the engine version, scripting API version, platform name, build type (development or release), current input mode and drawable size.

#### Scenario: Query platform
- **WHEN** a script reads `engine.info()`
- **THEN** it receives a table with `version`, `api`, `platform`, `build`, `input_mode` and `drawable_size`

### Requirement: Hot reload in development builds
In development builds a changed script file SHALL be reloaded within one second. A package's `on_reload` SHALL receive the state table the package registered for preservation. Release builds SHALL NOT watch files.

#### Scenario: Edit a script while running
- **GIVEN** a development build is running
- **WHEN** a package script file is saved with a change
- **THEN** within one second the package's scripts are reloaded and `on_reload` receives the preserved state

### Requirement: Runaway script protection
In development and test builds a single hook invocation exceeding an instruction budget (default 10 million VM instructions) SHALL be interrupted and reported with a traceback, as in error isolation.

#### Scenario: Infinite loop in a hook
- **WHEN** a hook enters an endless loop in a development build
- **THEN** the hook is interrupted, reported with a traceback and disabled, and the engine keeps running
