# asset-packages Specification

## Purpose
Defines how game content is organised and found: packages are plain folders beside the
binary, the base game is one of them, and mods can add to or override any of its assets.

## Requirements

### Requirement: Package discovery
The engine SHALL treat every folder under `assets/`, `mods/` next to the binary and the user's `mods/` folder as a package if it contains a `package.lua` manifest declaring `id`, `version`, `api` and `license`.

#### Scenario: Valid packages are found
- **GIVEN** `assets/base` and `mods/example` each with a valid manifest
- **WHEN** the engine starts
- **THEN** both packages are listed by `engine.packages()` with id, version and license

#### Scenario: Invalid manifest
- **GIVEN** a mod folder whose `package.lua` has a syntax error or lacks `id`
- **WHEN** the engine starts
- **THEN** that package is skipped with an error naming the folder and the problem
- **AND** all other packages load normally

### Requirement: Dependency-ordered loading
Packages SHALL load in a stable topological order of their declared dependencies, with ties broken by package id. Packages with missing or incompatible required dependencies, or in a dependency cycle, SHALL be disabled with a message.

#### Scenario: Dependent mod loads after base
- **GIVEN** a mod that requires `base >= 0.1`
- **WHEN** packages load
- **THEN** the mod loads after `base`

#### Scenario: Missing dependency
- **GIVEN** a mod that requires a package that is not installed
- **WHEN** packages load
- **THEN** the mod is disabled with a message naming the missing package and the game still starts

#### Scenario: Dependency cycle
- **GIVEN** two mods that require each other
- **WHEN** packages load
- **THEN** both are disabled with a message naming the cycle

### Requirement: Scripting API compatibility
A package SHALL be disabled with a clear message if its declared `api` major version differs from the engine's scripting API major version, or its minor version is newer than the engine's.

#### Scenario: Package targets a future API
- **GIVEN** the engine API is 1.2 and a package declares `api = "1.4"`
- **WHEN** packages load
- **THEN** the package is disabled and the message names both versions

### Requirement: Overlay lookup
A path such as `textures/carpet.png` SHALL resolve to the file in the last-loaded package that contains it, so later packages override earlier ones. Directory listings SHALL merge entries across packages.

#### Scenario: Mod overrides a texture
- **GIVEN** both `base` and a mod loaded after it contain `textures/carpet.png`
- **WHEN** the engine loads `textures/carpet.png`
- **THEN** the mod's file is used

#### Scenario: Merged listing
- **WHEN** a script lists `sounds/`
- **THEN** the result contains the union of files from all packages without duplicates

### Requirement: No content embedded in the binary
The engine executable and WebAssembly module SHALL contain no game assets. Missing assets SHALL be substituted at runtime by generated placeholders (checkerboard texture, unit cube model, silence) and reported once per path.

#### Scenario: Missing texture
- **WHEN** a script requests a texture path that no package contains
- **THEN** a generated checkerboard placeholder is returned
- **AND** one warning naming the path is logged, however often it is requested

### Requirement: Asynchronous loading
Asset requests SHALL return a handle immediately and complete asynchronously, observable by callback or by polling. The main thread SHALL NOT block on file or network reads in release builds.

#### Scenario: Load completes later
- **WHEN** a script requests a model
- **THEN** the call returns a handle in the `pending` state
- **AND** the handle reaches `ready` or `failed` in a later frame and the script's callback is invoked once

### Requirement: Web delivery and caching
On the web, packages SHALL be fetched over HTTP from the `assets/` path beside the page, guided by a generated index of paths, sizes and content hashes. Fetched files SHALL be cached by the browser and re-fetched only when their hash changes.

#### Scenario: Second visit uses the cache
- **GIVEN** a player has loaded the game once
- **WHEN** the page is reloaded and the asset index is unchanged
- **THEN** no asset file is downloaded again

#### Scenario: Updated asset
- **WHEN** an asset's hash in the index differs from the cached entry
- **THEN** that asset is downloaded again and the new version is used

### Requirement: Path safety
Package paths SHALL be normalised, and absolute paths, drive letters and `..` segments that would leave the package root SHALL be rejected.

#### Scenario: Escape attempt
- **WHEN** a script requests `../../secret.txt`
- **THEN** the request fails with an invalid-path error and no file outside the packages is read
