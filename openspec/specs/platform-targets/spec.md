# platform-targets Specification

## Purpose
Defines how the game is built, started and run on Windows, Linux, macOS and the Web, and how
it behaves when a platform lacks a required feature, so that one source tree serves every target.

## Requirements

### Requirement: Single source tree for all targets
The project SHALL build the engine for Windows (x64), Linux (x64), macOS (universal arm64 + x64), Web with threads and Web single-threaded from one source tree, using named build presets and no per-platform forks of game code.

#### Scenario: Each preset produces a runnable artifact
- **WHEN** CI builds the presets for Windows, Linux, macOS, web and web-st
- **THEN** each build succeeds without warnings
- **AND** each produces a runnable artifact (executable, app bundle, or HTML/JS/WASM set)

### Requirement: Startup locates content beside the binary
On start the engine SHALL locate the `assets/` folder next to the executable (inside `Contents/Resources` for a macOS app bundle, relative to the page URL on the web), load the packages and run the game entry script.

#### Scenario: Normal start
- **GIVEN** an `assets/base` package next to the executable
- **WHEN** the engine is started without arguments
- **THEN** the base package's entry script runs and the first frame is presented

#### Scenario: Missing content folder
- **GIVEN** no `assets/` folder next to the executable
- **WHEN** the engine is started
- **THEN** it shows a human-readable error naming the expected path (a message box on desktop, a page message on the web)
- **AND** it exits with a non-zero exit code without crashing

#### Scenario: Content path override
- **WHEN** the engine is started with `--assets <dir>`
- **THEN** packages are loaded from `<dir>` instead of the default location

### Requirement: Graphics capability check
The engine SHALL require OpenGL 3.3 core on desktop and WebGL 2 (OpenGL ES 3.0) on the web, and SHALL report a missing or insufficient graphics API as a readable error instead of crashing.

#### Scenario: Unsupported graphics context
- **WHEN** context creation fails or reports a version below the requirement
- **THEN** the engine shows an error naming the required and the detected version
- **AND** it exits with a non-zero exit code

### Requirement: Window handling
On desktop the engine SHALL open a resizable, high-DPI aware window, SHALL support toggling fullscreen at runtime, and SHALL report the drawable size in pixels to scripts after every change.

#### Scenario: Resize at runtime
- **WHEN** the window is resized or fullscreen is toggled
- **THEN** rendering continues without a restart
- **AND** scripts receive a resize event with the new drawable size in pixels

### Requirement: Per-platform user data location
The engine SHALL store configuration and saves in a per-user data location: `%APPDATA%/Zauberer` on Windows, `$XDG_DATA_HOME/zauberer` on Linux, `~/Library/Application Support/Zauberer` on macOS and IndexedDB-backed storage on the web, overridable with `--user-dir`.

#### Scenario: Data persists across runs
- **WHEN** a script writes a value through the save API and the engine is restarted
- **THEN** the value is read back unchanged on the same platform

#### Scenario: Override for tests
- **WHEN** the engine is started with `--user-dir <dir>`
- **THEN** all configuration and save reads and writes happen below `<dir>`

### Requirement: Web build selection by isolation support
The web shell SHALL load the multithreaded build when the page is cross-origin isolated and the single-threaded build otherwise, with identical game behaviour apart from performance.

#### Scenario: Static host without isolation headers
- **WHEN** the web build is served without COOP/COEP headers
- **THEN** the single-threaded build is loaded and the game starts

#### Scenario: Isolated host
- **WHEN** the page is cross-origin isolated
- **THEN** the multithreaded build is loaded

### Requirement: Orderly shutdown
On quit, by window close, script request or signal, the engine SHALL run script shutdown hooks, flush configuration and logs, release resources and exit with code 0.

#### Scenario: Script requests quit
- **WHEN** a script calls `engine.quit()`
- **THEN** shutdown hooks of all packages run in reverse load order
- **AND** the process exits with code 0

### Requirement: Assets read while the engine starts are preloaded in the browser
Files the engine reads while it starts (not on demand), such as the terrain atlas and its texture, SHALL be in the web build's boot set, so they are there when they are read.

#### Scenario: Assets read at start are there in the browser
- **WHEN** the game starts in the browser
- **THEN** no file read at start is missing or empty (the terrain atlas loads)
