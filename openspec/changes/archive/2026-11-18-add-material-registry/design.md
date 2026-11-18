# Design: Material Registry

## Context

Data files are read with the LUON parser (a literal subset of Lua, never executed). The
plant species registry (`data/flora.lua`) is one file that a mod can only replace as a
whole. The VFS knows the packages in load order and can read a file of a given package.

## Goals / Non-Goals

**Goals:**

- One merging mechanism for every registry (materials first; species, substances,
  reactions and weaves later).
- Material properties that heat, fire, water, gases and physics can use directly, in SI
  units.
- Clear messages for broken data, and the game keeps running.

**Non-Goals:**

- Substances and reactions (chemistry, M7).
- Moving `data/flora.lua` onto the merging registry; that follows with `add-ecology`.
- Rendering terrain from material colours: the terrain view keeps its atlas colours.
- Defining entries from Lua at run time.

## Decisions

### Merging in core, meaning in the consumer

`core/registry` knows only entries and fields. It deep-copies each package's entries
into its own arena, so the parsed documents can be freed. Patches replace fields one
level deep: a patched table field is replaced as a whole. That is simple to predict and
to log. The consumer (`sim/material`) validates fields and drops entries it cannot use.
This keeps the merge rules the same for every registry.

Keys are `<package>:<id>`. Ids use `a-z`, `0-9` and `_`. A bare id resolves when exactly
one package defines it, so base data can say `to = "water"`. A mod that adds its own
`water` makes bare `water` ambiguous. It then has to say `base:water`, and the engine's
lookup of air and water fails loudly instead of choosing at random.

### Dropping and re-resolving

Materials refer to each other (phase-change targets and combustion products). Building
the set parses every entry and resolves references to indices. It drops the first entry
that fails and starts again, until a pass has no failure. Indices shift when an entry is
dropped, and anything that pointed at the dropped entry fails on the next pass and is
reported too. With 250 entries at most this costs nothing.

### Phase changes

Each material lists up to four transitions:

- heating: `melts` (solid or powder to liquid) and `boils` (liquid to gas);
- cooling: `freezes` (liquid to solid or powder) and `condenses` (gas to liquid).

Each has a temperature and latent heat. Hysteresis is allowed: glass melts at 1400 °C,
but molten glass sets at 1000 °C. `zb_material_at_temperature` follows the chain a few
steps, so ice at 150 °C is steam.

### Units and ranges

SI units with °C. Ranges are wide but catch typos: density 0.01–25,000 kg/m³, and
specific heat 1–20,000 J/(kg·K). Hardness is on the Mohs scale, strength is compressive
strength in MPa, and friction is a coefficient up to 1.5.

## Risks / Trade-offs

- One-level patches cannot change a single product share. A mod overrides or patches the
  whole `burns` table instead. Good enough until mods ask for more.
- Bare ids becoming ambiguous when a mod reuses an id is a sharp edge. The engine's
  required materials report it clearly.
