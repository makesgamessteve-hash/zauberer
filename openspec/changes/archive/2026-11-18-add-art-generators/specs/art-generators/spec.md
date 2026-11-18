# Spec Delta: art-generators

## Purpose

Produces the game's procedural art reproducibly from generator tools in the repository and
defines the files the renderer reads: a curated palette, its colour lookup table, an
ordered-dither matrix, tileable terrain material textures and tile ornaments.

## ADDED Requirements

### Requirement: Reproducible art generators
The generators in `tools/gen/` SHALL use only the Python 3 standard library and SHALL write byte-identical files on every run, platform and Python version: fixed seeds, no time or environment input, integer arithmetic for every value that reaches an output, and no dependence on hash or set iteration order. `tools/gen/gen_all.py` SHALL regenerate every generated asset of the base package, given an output directory.

#### Scenario: Regenerate into an empty directory
- **WHEN** `tools/gen/gen_all.py` writes the base package's generated assets into an empty directory, in a fresh interpreter with a different string-hash seed
- **THEN** it writes the palette, lookup table, dither matrix, terrain atlas and ornament files and their Lua data files
- **AND** each of them is byte-identical to the committed file under `assets/base`

### Requirement: Curated base palette
The base package SHALL contain `palettes/base.png`, a 64 × 1 RGBA image of 64 distinct opaque colours. Every palette colour SHALL lie on the lookup table's 5-bit grid: each channel equals ⌊(255·v + 15) / 31⌋ for some level v from 0 to 31.

#### Scenario: Palette size
- **WHEN** `palettes/base.png` is read
- **THEN** it is 64 × 1 texels, and its 64 colours are distinct and have alpha 255

### Requirement: Palette ramps
`palettes/base.lua` SHALL return `colors = 64` and `ramps`: named lists of 1-based palette indices, each running from dark to light. The ramps SHALL together cover every palette index exactly once and SHALL include the families lapis, turquoise, saffron, pomegranate, ochre, green, brown, sand, stone, snow, water, sky and night.

#### Scenario: Ramps run dark to light
- **WHEN** the ramps of `palettes/base.lua` are read
- **THEN** they include the required families, cover the indices 1 to 64 exactly once, and each ramp has at least three colours whose Oklab lightness strictly increases

### Requirement: Palette lookup table
The base package SHALL contain `palettes/base_lut.png`, a 1024 × 32 RGBA image holding a 32 × 32 × 32 lookup table as 32 slices of 32 × 32 laid side by side. Texel (x, y) SHALL hold the palette colour nearest to the colour with 5-bit levels r = x mod 32, g = y and b = ⌊x / 32⌋, where level v stands for the 8-bit value ⌊(255·v + 15) / 31⌋. Nearest SHALL mean the smallest Euclidean distance in Oklab, ties going to the lower palette index.

#### Scenario: Palette colours map to themselves
- **WHEN** the table is read at the cell of each palette colour (each channel's level ⌊(31·value + 127) / 255⌋)
- **THEN** it holds that same palette colour

#### Scenario: Nearest colour in Oklab
- **WHEN** every cell is compared with a floating-point Oklab reference
- **THEN** each cell whose two nearest palette colours differ in distance by at least 10⁻⁶ holds the nearest one
- **AND** at least 32,000 of the 32,768 cells are checked this way

#### Scenario: Only palette colours in the table
- **WHEN** every texel of the table is read
- **THEN** each one is a palette colour with alpha 255

### Requirement: Ordered dither matrix
The base package SHALL contain `palettes/bayer8.png`, an 8 × 8 RGBA image of the standard 8 × 8 Bayer ordered-dither matrix, storing index i as R = G = B = 4·i + 2 with alpha 255.

#### Scenario: Bayer thresholds
- **WHEN** `palettes/bayer8.png` is read
- **THEN** its first row is 2, 130, 34, 162, 10, 138, 42, 170 and its last row is 254, 126, 222, 94, 246, 118, 214, 86
- **AND** it holds each value 4·i + 2 for i from 0 to 63 exactly once, in grey with alpha 255

### Requirement: Terrain material atlas
The base package SHALL contain `textures/terrain/atlas.png` with one 64 × 64 layer per terrain material, stacked vertically with layer i at y = 64·i, and `textures/terrain/atlas.lua`, which returns `size = 64`, the texture path and `layers`, the material names in strip order. The materials SHALL be grass, dry_grass, forest_floor, soil, sand, rock, gravel, snow, ice, mud, savanna and tundra, in that order.

#### Scenario: Layer list matches the strip
- **WHEN** `atlas.lua` and `atlas.png` are read
- **THEN** `layers` lists the twelve materials in order and the image is 64 texels wide and 64 × 12 texels tall

### Requirement: Seamless terrain layers
Every terrain layer SHALL tile seamlessly in both directions at a scale of about one texel per metre.

#### Scenario: Seamless terrain tiling
- **WHEN** each layer's lines are averaged over 7 texels along each wrap-around edge and the mean lightness difference across that edge is compared with the differences between neighbouring interior columns (or rows)
- **THEN** no edge exceeds 1.25 times the largest or 2 times the mean interior difference
- **AND** the same check rejects a layer whose lightness drifts by 0.19 across the tile

### Requirement: Readable terrain materials
Each terrain layer SHALL use between 3 and 10 palette colours, and the average colours of any two layers SHALL differ by at least 0.04 in Oklab, so materials stay distinguishable from a distance.

#### Scenario: Distinguishable materials
- **WHEN** the average colour of each layer is computed
- **THEN** every two layers' averages are at least 0.04 apart in Oklab

#### Scenario: Restrained shading
- **WHEN** the distinct colours of each layer are counted
- **THEN** every layer uses between 3 and 10 colours

### Requirement: Tile ornaments
The base package SHALL contain at least four tile ornaments in `textures/ornaments/`: `star8.png` (32 × 32, an eight-pointed star tile), `arabesque.png` (64 × 64, a scrolling vine), `kilim.png` (32 × 32, a kilim lozenge), each tileable in both directions, and `carpet_border.png` (64 × 16, a carpet border), tileable horizontally.

#### Scenario: Ornament set
- **WHEN** the ornaments directory is listed
- **THEN** it holds at least four PNG ornaments, and the four named ornaments have the stated sizes

#### Scenario: Seamless ornament tiling
- **WHEN** each ornament's wrap-around edges along its tiling directions are checked as for terrain layers
- **THEN** no edge exceeds 1.25 times the largest or 2 times the mean interior difference

### Requirement: Palette-only art
Every texel of the terrain atlas and of every ornament SHALL be an opaque colour of the base palette, so the art survives palette quantisation unchanged and palette swaps can recolour it.

#### Scenario: Every texel is a palette colour
- **WHEN** every texel of the terrain atlas and the ornaments is compared with the palette
- **THEN** each one is a palette colour with alpha 255

### Requirement: Generated art is licensed and credited
Every generated media file SHALL have an entry in the base package's `credits.lua` with author "Zauberer contributors", its generator script as source and license CC-BY-4.0. Every generated Lua data file SHALL start with an SPDX license comment for CC-BY-4.0, followed by the comment `-- Generated by tools/gen/<generator>.py. Do not edit.`

#### Scenario: Generated media are credited
- **WHEN** the base package's credits are read
- **THEN** the palette, lookup table, dither matrix, terrain atlas and every ornament have an entry naming their generator script as source, with license CC-BY-4.0

#### Scenario: Generated data files are marked
- **WHEN** `palettes/base.lua` and `textures/terrain/atlas.lua` are read
- **THEN** each starts with the CC-BY-4.0 SPDX line and the "Generated by" line naming its generator

### Requirement: Texture memory budget
The palette, lookup table, dither matrix, terrain atlas and ornaments SHALL together need at most 512 KiB as uncompressed RGBA8 textures, so they fit the mobile web budget.

#### Scenario: Art fits the budget
- **WHEN** width × height × 4 bytes is summed over these images
- **THEN** the total is at most 524,288 bytes
