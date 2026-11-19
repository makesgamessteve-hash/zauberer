# modding-sdk Specification

## Purpose
The modding SDK: the game's API for mods, the documented example mod, and packages the player turns off.

## Requirements

### Requirement: The example mod works
The example mod SHALL load after the base game and, through the game's API, add its item (an id taken once, items checked), give moth-wing dust at night, show its HUD line and message, and have German text merged with the base game's.

#### Scenario: The example mod works
- **WHEN** the game runs at night with the example mod installed
- **THEN** its item is in the pack, its line and message on the HUD, in English and German

### Requirement: A failing mod function is turned off
A function a mod gives the game that raises an error SHALL be turned off and reported once; the game goes on.

#### Scenario: A failing mod function is turned off
- **WHEN** a mod's tick function fails
- **THEN** it is called once, the error is reported, and play goes on

### Requirement: A package turned off
A package the player turned off SHALL be listed but disabled ("turned off") at the next mount, and packages that require it SHALL be disabled with it; the list is bounded and ignores empty or overlong ids.

#### Scenario: A package turned off
- **WHEN** a mod and one that requires it are mounted with the mod turned off
- **THEN** only the base game loads, the mod says turned off

### Requirement: Turning a package off from the game
engine.set_package_enabled SHALL turn a package off or on from the next start, keep the choice in config.lua, refuse the base game and unknown ids, and engine.packages() SHALL show the choice.

#### Scenario: Turning a package off from the game
- **WHEN** the example mod is turned off, the base game refused, and the game started again
- **THEN** the mod is turned off at the second start and the base game still loads

### Requirement: The example mod is tested
The example mod's own Lua tests SHALL pass with it installed, and its credits SHALL check.

#### Scenario: The example mod is tested
- **WHEN** its tests run
- **THEN** they pass
