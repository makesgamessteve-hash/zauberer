# Tasks

## 1. Registries

- [x] 1.1 `core/registry`: add, override and patch in load order, logs, lookups by key or bare id; verify "materials: Registries merge across packages" and "Broken entries are skipped"

## 2. Materials

- [x] 2.1 `sim/material`: properties, phase changes, combustion, mechanics, mana, colour; validation and dropping; required entries; verify "The base materials", "Phase changes by temperature" and "The engine's materials are required"
- [x] 2.2 `data/materials.lua` of the base game
- [x] 2.3 New `sim` module in CMake and the layering check

## 3. Engine and Lua

- [x] 3.1 The engine merges data/materials.lua from every package at start
- [x] 3.2 `registry.keys`, `registry.get`, `sim.material`, `sim.material_at`; fixture mod and scenario; verify "Materials from Lua and mods"
- [x] 3.3 Tag every scenario, regenerate the Lua API reference, keep coverage ≥ 95 %
