# Design: Climate and Biomes

## Context

The concept (§4.4) lists:

- insolation from latitude and season;
- temperature with lapse rate, ocean moderation and continentality;
- three-cell winds deflected by ranges;
- moisture advected from oceans, with orographic rain and rain shadows;
- simplified gyres (warm western boundary currents, cold eastern ones, coastal deserts);
- monthly temperature and precipitation per macro cell;
- hydrology with river order, salt lakes, glaciers and aquifers;
- Whittaker biomes refined by elevation, drainage, coast and seasonality;
- magical overlays.

Concept §10.3 adds biogeographic provinces that remember which landmasses were connected
and for how long.

## Goals / Non-Goals

**Goals:** plausible climate belts and contrasts that a player can read in the landscape,
fast enough for world creation, deterministic everywhere.

**Non-Goals:** weather (`add-weather`, M3), the visual seasons (`add-sky-and-time`),
species and their placement (M5), rivers and lakes as rendered water
(`add-water-bodies`), the world bundle file (`add-world-creation`).

## Decisions

### A coarser climate grid

Climate changes over hundreds of kilometres, so it runs on a cube grid at a quarter of
the macro resolution: 131 cells per face edge, about 24 km, 103,000 cells. That makes 12
monthly passes cheap. Macro cells sample it bilinearly on their own face (no
trigonometry) and apply the lapse rate for their own height.

### Temperature

T = −48 + 0.18·Q̄ + 8·sin²φ (heat transport) + swing·0.18·(Q_lagged − Q̄) + currents. Here
Q̄ is the annual mean insolation. The swing is 0.15 over the sea (a two-month lag) and
0.35–0.7 over land, growing with distance from the coast up to 1,200 km and damped
towards the poles. At sea level this gives about 27 °C at the equator, 12 °C at 45° and
−7 °C at 75°.

### Moisture

For every month, each cell takes its upwind neighbour (best aligned with the reversed
prevailing wind). In 128 Jacobi passes:

- every cell takes in its upwind moisture;
- it rains out a share given by the cell's rate (latitude belt, convection, orographic
  lift of +0.4 per km climbed, lee drying, very high ground);
- the ocean adds evaporation from its temperature;
- land returns 40 % of its rain.

This carries ocean moisture about 3,000 km inland, enough for the largest continents.
Cold currents lower evaporation, so subtropical west coasts are dry.

### Hydrology

Runoff = P²/(P + PET) with PET = 55·(T + 5) mm a year, limited to 0–1,800. Discharge
accumulates in Kahn order from the headwaters along the geology's receivers. A lake cell
subtracts its evaporation, and a lake whose inflow it all evaporates becomes a salt lake
with nothing flowing on. Strahler orders follow the river cells.

### Biomes by precedence

Biomes are decided in this order:

1. ocean (by depth, temperature and sea ice);
2. lakes;
3. glaciers;
4. great rivers;
5. coast (estuary, mangrove, cliffs, dunes, salt marsh, beach);
6. volcanic slopes;
7. mountains (cloud forest, alpine meadow, scree, montane forest);
8. salt flats over geological salt;
9. wetlands;
10. the Whittaker rule.

A seeded hash chooses between rocky and sand deserts and between reef and kelp. Blending
across borders is left to the chunk layer, which reads neighbouring cells.

### Magical biomes

Only nexuses of strength ≥ 0.75 make magical land, in a radius of 15–60 km. The setting
picks the overlay:

| Setting | Overlay |
| --- | --- |
| Volcano | Ashlands |
| Strongest triple junction | The Hollows |
| Wetland or water | Drowned Vale, or Mirefen where it is hot |
| Rainforest | Wildheart |
| Arid highland | Stormspires |
| Mana crystals | Glasswood |
| Suture or high ground | Deepstone |

The real mana fields come with `add-mana-fields` (M4) and may move these borders.

### Provinces

Seeds go on each large landmass: for every lineage holding at least 5 % of it, or
300,000 km², one seed per 2.5 million km², at most six. Seeds sit at evenly spaced ranks
of that lineage's cells, in cell order. Growth is a Dijkstra search with these costs:

| Step | Cost |
| --- | --- |
| Ordinary land | 1 |
| High mountains (above 2.5 km) | +4 |
| Deserts | +2 |
| Ice | +4 |
| A lineage border | +25, unless the lineages met within 50 Myr |
| A coastal water cell (at most two from land) | 12 |

Islands that are never reached become their own provinces. Isolation is the larger of
two values: island smallness, and a lineage that never met another (0.6) or last met one
over 100 Myr ago (0.3).

## Risks / Trade-offs

- [Single upwind neighbour advection is directional on the grid] → winds mix three
  components and 8-neighbour choice; climate belts are broad enough at 24 km cells.
- [Tuning constants shape every world] → golden hashes pin them; Earth-like shares were
  checked on default worlds (deserts about a quarter of land, ice sheets 6–14 %, lakes
  1–2 %).
- [Macro-cell rivers start at 8 m³/s] → smaller streams belong to the region layer.
