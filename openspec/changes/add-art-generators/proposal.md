# Proposal: Art Generators

## Why

Milestone **M1 (First Flight)** needs the retro look of the concept (§8.1 of
`docs/concept/08-presentation-controls.md`). The look is "1996, seen through modern eyes":
point-sampled pixel-art textures, post-process quantisation to a curated palette with
ordered dithering, and an Oriental colour world of lapis, turquoise, saffron, pomegranate
and ochre. The renderer change `add-retro-renderer` consumes a palette, a lookup table that
maps any colour to the palette, a dither matrix and terrain material textures. Today none
of these exist.

The project is procedural-first: generator tools in the repository write most art
reproducibly into packages as ordinary CC BY 4.0 files, so the art stays moddable, reviewable
as code and cheap to change. This change starts that pipeline with the palette, the
terrain materials and the first tile ornaments.

## What Changes

- Add **Python generators** in `tools/gen/` (standard library only, like the existing
  font, sprite, carpet and sound generators). Running `tools/gen/gen_all.py` regenerates
  every generated asset of the base package byte-identically:
  - `gen_palette.py` writes the palette, its lookup table and the dither matrix,
  - `gen_terrain.py` writes the terrain material atlas,
  - `gen_ornaments.py` writes the tile ornaments,
  - `tilenoise.py` is a shared library of tileable, integer-only noise,
  - `pngw.py` gains a PNG reader for the tests.
- Add a **curated 64-colour palette** (`palettes/base.png`, `palettes/base.lua`). It is
  built from ramps (dark to light) per hue family: the Oriental accents, plus greens,
  browns, sand, rock greys, snow, water, sky, night and violet.
- Add a **32 × 32 × 32 lookup table** (`palettes/base_lut.png`). It maps every colour to
  its nearest palette colour in Oklab.
- Add an **8 × 8 Bayer dither matrix** (`palettes/bayer8.png`).
- Add **twelve tileable 64 × 64 terrain materials**, stacked in one atlas strip
  (`textures/terrain/atlas.png`, `atlas.lua`): grass, dry grass, forest floor, soil, sand,
  rock, gravel, snow, ice, mud, savanna and tundra.
- Add **four tile ornaments** (`textures/ornaments/`): an eight-pointed star-and-cross
  tile, an arabesque scroll, a kilim lozenge and a carpet border.
- Credit every new media file in `assets/base/credits.lua`. Test the art with Python unit
  tests: reproducibility, palette and lookup-table correctness, tiling, distinct
  materials, palette-only texels, credits and a texture-memory budget.

## Capabilities

### New Capabilities

- `art-generators`: reproducible, palette-based procedural art for the packages and the
  file contracts the renderer reads (palette, lookup table, dither matrix, terrain atlas,
  ornaments).

### Modified Capabilities

None. The new files fall under the existing `content-licensing` rules (credits, REUSE
metadata), which need no change.

## Impact

- **New files:** `tools/gen/{gen_palette,gen_terrain,gen_ornaments,tilenoise}.py`,
  `assets/base/palettes/`, `assets/base/textures/terrain/`,
  `assets/base/textures/ornaments/`, `tests/tools/test_art_generators.py`.
- **Changed files:** `tools/gen/gen_all.py` (calls the new generators), `tools/gen/pngw.py`
  (PNG reader), `assets/base/credits.lua` (eight new entries).
- **Engine code:** none. The renderer reads these files in `add-retro-renderer`.
- **Platforms:** no differences. Desktop, web, mobile web and WebXR load the same files
  from the base package; the web build fetches them on demand like any other asset. The
  new textures need about 350 KiB of RGBA texture memory, within the 512 KiB budget in
  the spec.
- **Co-op:** none. The art is static package content, identical on every peer because it
  ships with the package.
- **CI:** the existing "Generated assets are reproducible" step (`gen_all.py` followed by
  `git diff --exit-code -- assets`) now covers the new art. The tool tests run in the
  existing `Tool tests` step.
