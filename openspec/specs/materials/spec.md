# materials Specification

## Purpose
Defines how data registries merge across packages (add, override, patch) and the
material registry the simulations share: what every solid, powder, liquid and gas is
made of, how it changes phase, burns, wears and conducts mana.

## Requirements

### Requirement: Registries merge
Registries SHALL merge the data entries of every package in load order: an entry with an id adds "<package>:<id>", an override replaces an existing entry and a patch changes the fields it gives; overrides and patches SHALL be logged, and an id without a package SHALL name the entry only one package defines.

#### Scenario: Registries merge across packages
- **WHEN** the base package defines entries and a mod adds an entry with a known id, patches one base entry and overrides another
- **THEN** the patched entry keeps its other fields, the overridden entry has only the mod's fields, the bare id becomes ambiguous, and both changes are logged with the fields patched

### Requirement: Broken entries are skipped
An entry that cannot be used SHALL be skipped with a warning naming the package or entry and the reason, and the rest of the registry SHALL still load; running out of memory SHALL be reported.

#### Scenario: Broken entries are skipped
- **WHEN** data has entries that are not tables, lack an id, have invalid or duplicate ids, change entries that do not exist, or materials with missing, mistyped or out-of-range fields, unknown references or impossible phase changes
- **THEN** each is skipped with its own message, materials that refer only to skipped ones are skipped as well, and the valid entries remain

### Requirement: Material properties
Every material SHALL have a name, a phase (solid, powder, liquid or gas), density, specific heat, conductivity and a colour, and MAY have phase changes with temperature and latent heat, combustion with products, hardness, strength, friction and mana conductivity and affinities to the eight colours.

#### Scenario: The base materials
- **WHEN** the base game's materials are loaded
- **THEN** every entry is valid, every phase has several materials, water has its real density and heat capacity and freezes and boils with latent heat, wood burns into charcoal and ash, and stone does not burn

### Requirement: Phase changes
The material a substance becomes at a temperature SHALL follow its phase changes: melting and boiling when heated past their temperatures, freezing and condensing when cooled below them, over several steps.

#### Scenario: Phase changes by temperature
- **WHEN** water, ice, steam, granite, lava and sand are taken to other temperatures
- **THEN** water freezes to ice, ice turns to steam above boiling, steam turns to ice below freezing, granite melts to lava that cools into basalt, and sand melts and cools into glass

### Requirement: Required materials
Building the materials SHALL fail with a message when air, water or any terrain surface material (soil, sand, rock, gravel, snow, ice, mud) has no usable entry, and SHALL keep at most 250 materials.

#### Scenario: The engine's materials are required
- **WHEN** any one of the required materials is missing, or more than 250 are defined
- **THEN** building fails naming the missing material, or the extra entries are left out

### Requirement: Materials in the game
The engine SHALL merge data/materials.lua from every package at start, and scripts SHALL read the merged entries with their key and packages, the simulated properties of a material and the material it becomes at a temperature.

#### Scenario: Materials from Lua and mods
- **WHEN** a mod adds a material and patches a base one, and a script reads the registry
- **THEN** the mod's material is listed after the base ones, the patched entry shows the new value and the patching package, sim.material gives the properties with phase changes and products as keys, and unknown kinds and materials are refused
