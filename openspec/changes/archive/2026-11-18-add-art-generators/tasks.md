# Tasks

## 1. Palette, lookup table and dither matrix

- [x] 1.1 Add `tools/gen/gen_palette.py` with the 64-colour palette in 15 dark-to-light ramps, snapped to the 5-bit grid, writing `palettes/base.png` and `palettes/base.lua`; verify the tests "art-generators: Palette size" and "Ramps run dark to light"
- [x] 1.2 Implement integer Oklab (decimal sRGB table, exact integer cube roots) and write `palettes/base_lut.png`; verify the tests "Palette colours map to themselves", "Nearest colour in Oklab" (against a floating-point reference) and "Only palette colours in the table"
- [x] 1.3 Write `palettes/bayer8.png` from the recursive Bayer construction; verify the test "Bayer thresholds"
- [x] 1.4 Add `read_png` to `tools/gen/pngw.py` for the tests; verify it decodes the existing font and sprite PNGs

## 2. Terrain materials

- [x] 2.1 Add `tools/gen/tilenoise.py` (integer hash, tileable value noise and fBm, toroidal Worley noise with a de-duplicated 5 × 5 search, domain warp, rank quantisation)
- [x] 2.2 Add `tools/gen/gen_terrain.py` with recipes for grass, dry grass, forest floor, soil, sand, rock, gravel, snow, ice, mud, savanna and tundra, writing `textures/terrain/atlas.png` and `atlas.lua`; verify the tests "Layer list matches the strip" and "Restrained shading"
- [x] 2.3 Tune the recipes until every pair of layer averages is at least 0.04 apart in Oklab and every wrap-around edge passes the seam check; verify the tests "Distinguishable materials" and "Seamless terrain tiling" (including the drifting-texture negative control)

## 3. Ornaments

- [x] 3.1 Add `tools/gen/gen_ornaments.py` with the star-and-cross tile, the arabesque scroll, the kilim lozenge and the carpet border in integer geometry; verify the tests "Ornament set" and "Seamless ornament tiling"
- [x] 3.2 Check that the atlas and every ornament use only palette colours; verify the test "Every texel is a palette colour"

## 4. Pipeline, licensing and budget

- [x] 4.1 Call the new generators from `tools/gen/gen_all.py`; verify the test "Regenerate into an empty directory" (fresh interpreter, different hash seed) and that `python3 tools/gen/gen_all.py && git status --short assets` leaves the M0 assets unchanged
- [x] 4.2 Credit every new media file in `assets/base/credits.lua` and mark the generated Lua files; verify the tests "Generated media are credited" and "Generated data files are marked", plus `tools/check_credits.py`, `tools/check_licenses.py`, `reuse lint` and `luacheck`
- [x] 4.3 Verify the test "Art fits the budget" (at most 512 KiB of RGBA8 texture memory)
- [x] 4.4 Map every scenario of this change to a tagged automated test in `tests/tools/test_art_generators.py` (`tools/spec_coverage.py` reports none of them uncovered)
