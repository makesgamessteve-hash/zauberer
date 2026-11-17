# 4. World Generation

## 4.1 Principles

1. **Lazy.** Nothing is generated before it's needed. Detail is generated in layers, from
   coarse to fine, as the player approaches.
2. **Deterministic.** Every layer is a pure function of *(world seed, generator version,
   coordinates, coarser layers)*. The same seed gives the same world on every platform,
   including the web build. Generated data is never saved, only player-caused and
   simulation-caused **changes** (deltas) are.
3. **Consistent at every scale.** Local detail follows from global structure. A desert
   exists because of a rain shadow behind a mountain range, which exists because of a
   plate collision. A river always has a source and a mouth.
4. **Realistic first, magical second.** Earth-like physical geography comes first. Magical
   phenomena such as floating islands, crystal forests and fungal jungles are overlays
   where the mana field is extreme.

## 4.2 The planet

- **Shape:** a sphere represented as a **cube-sphere**: six cube faces projected onto the
  sphere with an area-balancing mapping to limit distortion. Each face is subdivided into
  a quadtree. The leaves at full detail are **chunks** of 256 m × 256 m.
- **Size:** the default radius is **2,000 km**, which gives about 12,500 km of equator.
  Circling the planet by carpet takes 100+ hours. This is effectively endless while
  staying globally consistent. Radius is a world-creation option, from 50 km (small test
  worlds) to 6,371 km (Earth size).
- **Orbit and rotation:**
  - **Axial tilt** (default 23°) creates seasons, polar day and night, and climate bands.
  - **Day length** defaults to 40 real minutes per game day and is configurable.
  - **Year length** defaults to 4 seasons × 12 days.
  - A **moon** drives tides (visual only), night light and ritual timing.
- **Coordinates:** a position is *(face, chunk x, chunk y, local x/y/z as float)*. Physics
  and rendering use a **floating origin**: everything is relative to an anchor near the
  camera, which gets re-anchored as the player moves. This keeps single-precision floats
  accurate everywhere.
- **Curvature:** gameplay inside the active area treats the ground as locally flat, with
  gravity along the local normal. Rendering bends distant geometry below the horizon, so
  the planet looks round from altitude.

> **Why a planet and not an infinite plane?** An infinite plane (as in Minecraft) can't
> have consistent climate bands, poles, ocean currents or rain shadows. Its biomes are
> noise. A planet gives real geography, and at 2,000 km radius it is still endless for
> practical play. The cost is more complex coordinates. Both alternatives are recorded in
> the implementation plan.

## 4.3 The generation pyramid

| Layer | Extent | When generated | Contents |
| --- | --- | --- | --- |
| **Planet** | Whole globe, ≈ 6 km macro cells | Once, at world creation (seconds; progress screen "The world is being woven…") | Plates, elevation, climate, rivers, biomes, geology provinces, ley network, history, factions |
| **Region** | 16 × 16 chunks ≈ 4 km | When the player comes within ≈ 20 km | River and lake shapes, roads, settlement and structure plans, cave networks, story set-piece placement, magical anomalies |
| **Chunk** | 256 m × 256 m | In stages as the player approaches (see 4.6) | Terrain heights, surface materials, strata, vegetation, props, structure shells, creature populations |
| **Detail** | Building, cave or room | On approach (< 100 m) or entering | Interiors, furnishing, books, loot, NPC households |

Each layer only reads from coarser layers and from its own seeds. Structures that span
chunk borders, such as a castle or a town, are **planned at the region level** and
**realized per chunk**, so they never get cut at seams.

## 4.4 Planet layer (global model)

### Tectonics (simplified)

- 12–40 plates from a spherical Voronoi partition. Each plate has a random rotation pole
  and speed, and a crust type (continental or oceanic).
- Relative motion at boundaries determines the boundary type:
  - **Convergent:** fold mountains (continent–continent), volcanic arcs and trenches
    (ocean–continent), island arcs (ocean–ocean).
  - **Divergent:** rift valleys and mid-ocean ridges.
  - **Transform:** fault lines and offset ridges.
- **Hotspots** create volcanic island chains along a plate's motion.
- Age per region gives old, eroded mountains and young, jagged ones.

### Elevation and erosion

- Base elevation by crust type, plus uplift profiles at boundaries, plus multi-scale noise.
- **Macro erosion** runs a stream-power iteration and thermal erosion on the macro grid.
  This gives realistic drainage networks, valleys, foothills and alluvial plains.
- **Sea level** is chosen to hit a target land fraction (default 35 %).

### Climate

- **Insolation** depends on latitude and season, from the axial tilt.
- **Temperature** comes from insolation, the altitude lapse rate (≈ 6.5 °C/km), the
  moderating effect of oceans and continentality (distance from sea).
- **Prevailing winds** follow a three-cell model: trade winds, westerlies and polar
  easterlies. Large mountain ranges deflect them.
- **Moisture** is advected along the winds from evaporating oceans. Orographic rain falls
  on windward slopes, and **rain shadows** form on leeward slopes.
- **Ocean currents** form simplified gyres: warm currents along western ocean boundaries
  and cold currents along eastern ones. Cold currents produce coastal deserts and fog.
- The result is monthly temperature and precipitation per macro cell, which feeds biomes,
  weather and seasons.

### Hydrology

- **Flow accumulation** on the eroded terrain gives rivers with an order (creek → great
  river), river mouths, deltas and estuaries.
- Depressions are filled until they spill, which forms **lakes**. Basins without an
  outflow become salt lakes and salt flats.
- **Glaciers and ice caps** form where it is cold and there is snowfall.
- **Aquifers** (groundwater depth) are recorded for springs, wells and oases.

### Geology and minerals

- **Rock provinces:** igneous (volcanic arcs, hotspots), sedimentary (basins, shelves,
  deltas) and metamorphic (old mountain belts). Strata sequences per province are used
  when digging.
- **Mineral deposits** follow geological logic:
  - copper, gold and silver in veins near igneous intrusions,
  - iron in sedimentary bands,
  - salt and gypsum in evaporite basins,
  - sulfur near volcanoes,
  - coal in old swamp basins,
  - gemstones and **mana crystals** in metamorphic and nexus zones,
  - clay in floodplains,
  - limestone (and therefore **karst caves**) on old marine shelves.

### Biomes

Biomes are classified with a **Whittaker scheme** (annual temperature × annual
precipitation), refined by elevation, drainage, soil, coast and seasonality. Biomes blend
across transition zones instead of switching hard at borders.

| Group | Biomes |
| --- | --- |
| Aquatic | Deep ocean, continental shelf, coral reef, kelp forest, lakes, rivers |
| Coastal | Beaches, dunes, cliffs, mangroves, salt marsh, estuaries |
| Tropical | Rainforest, seasonal forest, cloud forest, savanna |
| Arid | Sand desert (ergs with wind-aligned dunes), rocky desert (regs, mesas), salt flats, xeric shrubland |
| Temperate | Mediterranean scrub, steppe and prairie, deciduous forest, mixed forest, temperate rainforest |
| Cold | Taiga (boreal forest), tundra, polar desert, ice sheet |
| Mountain | Montane forest, alpine meadow, scree and cliffs, glaciers, volcanic slopes |
| Wetland | Marsh, swamp forest, peat bog, floodplain |
| **Magical** (overlays where mana is extreme) | **Ashlands** (Fire: smouldering ground, fire-adapted life), **Stormspires** (Storm: lightning-scarred mesas, crackling air), **Glasswood** (Light: crystal-grown forest), **Wildheart** (Life: giant overgrowth), **Drowned Vale** (Water: hanging mists, levitating water), **Deepstone** (Earth: monolith fields, gravity wells), **The Hollows** (Void: **floating islands**, distorted distances), **Mirefen** (Decay: giant fungal forests, spore fog) |

### Mana and ley network

- Base mana fields per color are derived from biome, geology, water and life (see
  [Magic](03-magic.md)).
- A **ley graph** connects nexus candidates, which are tectonic triple junctions,
  volcanoes, great river confluences and extreme mana cells. It is built as a sparse
  spanning network with extra loops, and its edges follow ridges and faults.

### History and factions

A coarse **world history** is simulated at world creation. It produces settlements,
roads, castles, ruins, cultures and lore books. See
[Settlements and architecture](06-settlements-architecture.md).

## 4.5 Region layer

Generated from the planet data when the player comes within about 20 km:

- **Rivers** are refined into splines with meanders, oxbow lakes, waterfalls at steep
  drops, and rapids. Widths and depths come from discharge.
- **Lake shorelines**, coastlines refined with bays, spits and cliffs.
- **Roads and trails** connect settlements with terrain-aware pathfinding. Bridges and
  fords sit at river crossings.
- **Settlement plans:** footprints of districts, plots, walls and fields.
- **Structure placements:** castles, towers, monasteries, mines, ruins, shrines,
  waystones and **story set pieces** reserved by the narrative director.
- **Cave networks:** a graph of chambers and tunnels, depending on geology (karst caves,
  lava tubes, sea caves, old mines). The graph records their surface entrances.
- **Magical anomalies:** floating island clusters, crystal outcrops and fungal groves.
- **Exact ley line paths** and nexus sites.

## 4.6 Chunk layer (staged)

A chunk advances through stages. Each stage requires certain stages of itself and,
sometimes, of its neighbours. Distant chunks stop early. This is the core of lazy
generation.

| Stage | Produces | Needed for |
| --- | --- | --- |
| **0 Coarse** | Height and color from planet/region data at low resolution | Far horizon (5–20 km) |
| **1 Terrain** | 1 m heightfield with detail noise conditioned on biome and geology | Near terrain mesh, collisions |
| **2 Surface** | Surface material per cell (soil, sand, rock, snow, mud, gravel, ice); river beds carved; strata columns | Texturing, digging, fluids |
| **3 Ecology** | Plant instances (Poisson-disk sampling weighted by species suitability), deadwood, fungi patches, rocks and props; per-chunk animal population counts | Vegetation rendering, foraging |
| **4 Structures** | Shells of buildings, walls and ruins realised from region plans; sparse voxel volumes for caves, overhangs and floating islands | Exteriors, navigation |
| **5 Active** | Simulation state is initialised: temperature, moisture, fluids, individual creatures spawned from populations | Full simulation (active tier) |

**Terrain detail** uses noise conditioned on biome and geology:

- ridged multifractals for young mountains,
- smooth hills for old ones,
- dunes aligned to the prevailing wind,
- mesas and terraces in arid sedimentary rock,
- U-shaped valleys and moraines where glaciers were,
- sinkholes in karst,
- lava flows and cones in volcanic areas,
- and a cheap analytic **erosion filter** that gives slopes gullies and ridgelines
  without iterative simulation.

**Overhangs, caves and floating islands** use **sparse voxel volumes** (1 m voxels,
0.5 m for detail) attached to chunks only where needed. The heightfield gets holes where
a volume takes over, for example at cave mouths.

## 4.7 Streaming and level of detail

| Ring | Distance (typical, configurable per platform) | Chunk stage | Rendering |
| --- | --- | --- | --- |
| Active | 0–384 m | 5 | Full detail; every plant, creature and particle |
| Near | 384 m–2 km | 3–4 | Reduced meshes; vegetation sprites and clumps; simplified creatures |
| Far | 2–8 km | 1–2 | Coarse terrain meshes; forest canopy surfaces; settlement silhouettes |
| Horizon | 8–20 km+ (when flying high) | 0 | Planet data meshes, fog, curvature |

- **Priority queue:** generation jobs are ordered by distance, view direction and
  **predicted position**. A fast carpet looks ahead along its velocity. Jobs that fall out
  of range are cancelled.
- **Budgets:** time per frame on the main thread, a worker-thread pool, and memory caps per
  platform (desktop, web, mobile web). An LRU cache evicts chunks that are both distant
  and unmodified. They are regenerated if needed, and generated data never touches the
  disk.
- **Hysteresis** on all ring borders prevents thrashing when the player hovers at a
  border.

## 4.8 Determinism and versioning

- Randomness comes from **counter-based hashing** (seed + layer id + coordinates + purpose)
  instead of stateful generators, so generation order and threading never change results.
- World generation follows strict floating-point rules: no fused multiply-add
  contraction, own implementations of transcendental functions, and no platform-dependent
  library math. **Golden hash tests** on every platform in CI verify this.
- Each world records the **generator version** it was created with. Released generator
  versions are immutable. Improvements ship as new versions, and existing saves keep their
  version, because unmodified chunks are regenerated on demand and must not change under
  the player's feet. Mods that alter generation declare this, and are recorded per world.

## 4.9 World creation options

These are the options on the "new world" screen:

- seed,
- planet radius,
- land fraction,
- global temperature offset (ice age to hothouse),
- magic intensity (mana abundance and the number of magical biomes),
- history depth (number of simulated centuries, which means more ruins and lore),
- survival difficulty,
- story on or off (a pure sandbox is possible).
