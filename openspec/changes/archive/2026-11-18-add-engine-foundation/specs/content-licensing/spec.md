# Spec Delta: content-licensing

## Purpose

Makes the project's licensing verifiable: code under MIT, creative assets under CC BY 4.0,
every file covered by license metadata and every asset credited, both in the repository and in the game.

## ADDED Requirements

### Requirement: License metadata for every file
Every file in the repository, apart from generated build output, SHALL be covered by license metadata: an SPDX header or an entry in the repository's license map. The license texts SHALL be present in `LICENSES/`.

#### Scenario: File without license information
- **WHEN** a file without an SPDX header and without a license-map entry is added
- **THEN** the licensing lint in CI fails and names the file

### Requirement: Project license split
Source code, scripts, shaders and build files authored for the project SHALL be licensed MIT. Textures, sprites, models, voxel models, audio, music, project fonts and story text SHALL be licensed CC BY 4.0.

#### Scenario: Asset declared with the wrong license
- **WHEN** a project-authored texture is declared as MIT in the license map
- **THEN** the licensing check fails and names the file and the expected license

### Requirement: Asset attribution
Each package SHALL contain a machine-readable credits file that lists every asset file with author, source and license. CI SHALL fail for any asset without a credits entry and for any entry pointing to a missing file.

#### Scenario: New sprite without credit
- **WHEN** a sprite is added to `assets/base/sprites/` without a credits entry
- **THEN** the credits check fails and names the file

### Requirement: Third-party asset license allowlist
Third-party assets SHALL only be accepted under CC0 or CC BY 4.0 compatible licenses, unless an exception with a written rationale is recorded in the credits file.

#### Scenario: Incompatible third-party asset
- **WHEN** an asset credited under a non-commercial license is added without a recorded exception
- **THEN** the credits check fails and names the asset and its license

### Requirement: In-game credits
The game SHALL provide a credits screen generated from the credits files of all loaded packages, including mods, and from the third-party code notices.

#### Scenario: Mod assets appear in credits
- **GIVEN** a mod with two credited assets is loaded
- **WHEN** the credits screen data is requested by scripts
- **THEN** it contains both assets with author, source and license under the mod's name
