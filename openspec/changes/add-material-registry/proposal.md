# Proposal: Material Registry

## Why

M3 builds the simulation backbone: heat and fire, shallow water, gases, weather,
deformation and rigid bodies. All of them ask the same questions of whatever is in a
cell: how dense, how much heat it holds and conducts, when it melts, boils or burns, and
what it burns into (concept 5.4). Concept 9.3 wants these as data that packages
merge, so that mods add materials without touching C.

## What Changes

- **Registries that merge** (`core/registry`). A package's data file returns a list of
  entries. `{ id = ... }` adds `<package>:<id>`, `{ override = ... }` replaces an
  entry, and `{ patch = ... }` changes some of its fields. Patches and overrides are
  logged; broken entries are skipped with a warning naming the package and the entry.
  A reference may leave out the package when only one package defines that id.
- **Materials** (`sim/material`, the first part of the new `sim` module):
  - phase (solid, powder, liquid or gas), density, specific heat and conductivity;
  - phase changes (melts, boils, freezes, condenses) with temperature, target and
    latent heat, checked for sensible phases;
  - combustion: flammability, ignition, heat of combustion, burn rate and up to four
    products;
  - hardness, strength and friction;
  - mana conductivity and affinity to the eight colours;
  - a palette colour.
  - Entries with a missing or wrong field, an unknown material or a wrong phase change
    are left out with a message naming the entry and the field. Air, water and the
    terrain's surface materials must be there.
- **Data**: `data/materials.lua` of the base game: 37 materials, from air to bone,
  with values simplified from real tables.
- **Engine**: materials are merged from every package in load order at start.
- **Lua**: `registry.keys(kind)` and `registry.get(kind, ref)` read the merged entries,
  `sim.material(ref)` gives what the simulations use, and `sim.material_at(ref, t)`
  follows phase changes.

## Capabilities

### New Capabilities

- `materials`: registries that merge across packages, and the material registry.

### Modified Capabilities

None.

## Impact

- New: `src/core/registry.{h,c}`, `src/sim/` with `material.{h,c}`,
  `src/script/script_sim.c`, `assets/base/data/materials.lua`, tests
  `test_core_registry`, `test_material`, a fixture mod and a scenario.
- Changed: the engine (loading the registry), the script host (the `registry` and `sim`
  globals), the layering check (`sim` sits on `core` and `world`), luacheck globals.
- Platforms: parsing 36 entries takes well under a millisecond at start.
- Co-op: peers already need the same package set, so they merge the same registry.
