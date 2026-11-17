# 5. Simulation: Physics, Chemistry and Biology

## 5.1 Approach

The goal is **plausible emergent behaviour at a playable cost**, not scientific accuracy.

- **Heuristic models.** Each model is the simplest one that produces the right visible
  behaviour: cellular automata, coarse grids, analytic shortcuts and population equations.
- **Data-driven.** Materials, substances, reactions, species and their parameters are
  data in Lua packages. Mods can add new ones without touching C.
- **Tiered.** Detail depends on distance to the player (5.2).
- **Multi-rate.** Each subsystem ticks at its own frequency (5.3).
- **Deterministic.** The same inputs give the same results. This is required for replays,
  automated tests and debugging.

## 5.2 Simulation tiers

| Tier | Where | What is simulated |
| --- | --- | --- |
| **Active** | ≈ 384 m around the player | Everything: field grids (heat, moisture, gases), shallow-water fluid, fire cells, individual plants' states, individual creatures with full AI, physics objects, particles |
| **Near** | ≈ 384 m – 2 km | Coarser fields (4× cell size); fire as fronts; water at rest except rivers; creatures as simplified agents (move along the navigation graph, no fine perception) |
| **Far** | Loaded and visible, beyond near | Aggregates per chunk: population counts, vegetation density and growth stage, burning or not, wetness; no individuals |
| **Dormant** | Visited earlier, now unloaded | Frozen with a timestamp. On reload a **catch-up** step applies closed-form evolution over the elapsed time |
| **Global** | Whole planet, macro grid | Weather systems, seasons, ley network, migrations, settlements and factions, rival mages, Sereth's campaign, economy |

### Catch-up (time skip) model

When a dormant chunk is reloaded after Δt, these steps run in order:

1. **Fires:** a fire that was burning is resolved analytically. The burnt area is
   estimated from fuel load, wind history (from the global weather record) and rain.
   The result is applied as burnt cells.
2. **Vegetation:** plant growth and succession advance by Δt.
3. **Populations:** creature populations follow their population equations, using
   carrying capacities from the current vegetation.
4. **Decay:** carcasses rot, dropped food spoils, and snow melts or accumulates according
   to the season.
5. **Settlements:** the global settlement and faction state is applied, for example
   repairs and new buildings.

Catch-up is approximate by design. It only has to be **plausible and consistent**.

## 5.3 Multi-rate scheduler

| Subsystem | Rate (sim ticks are fixed, game-time scaled) |
| --- | --- |
| Player, carpet and rigid-body physics | 60 Hz |
| Projectiles and spell effects | 60 Hz |
| Heat, gases, fire, shallow water (active tier) | 20 Hz |
| Creature AI decisions (perception is event-driven) | 5 Hz, staggered |
| Plant state, ecology in the active and near tiers | 1 Hz, staggered across chunks |
| Weather (local manifestation) | 1 Hz |
| Global macro simulation | once per game hour, spread over frames |

Rendering interpolates between simulation states, so the visible frame rate is
independent of the tick rate.

## 5.4 Materials

Every solid, liquid and gas in the world is an entry in the **material registry**:

- density, phase (solid, powder, liquid or gas) and transition temperatures (melting and
  boiling points with latent heat),
- specific heat and thermal conductivity,
- combustion: flammability, ignition temperature, heat of combustion, burn rate, and
  products (ash, smoke, CO₂ and steam),
- mechanical properties: hardness (for digging and mining), strength (for structural
  support) and friction,
- chemical properties: links to the substance registry for reactions,
- magical properties: mana conductivity and affinity per color,
- rendering properties: texture, palette entry, sprite set and sounds.

Typical materials are soils, sand, clay, gravel, stone types, ores, wood types, leaves and
grass, thatch, cloth (the carpet!), metals, glass, water, ice, snow, mud, lava, steam,
smoke, air, flesh and bone, and fungal matter. Values are taken from real tables and
simplified.

## 5.5 Heat and temperature

- **Field:** a temperature per cell. In the active tier the grid is a 2.5D surface layer
  plus a few air layers, using 4 m cells for air and the 1 m terrain cells for the
  surface. Enclosed spaces such as interiors and caves use a sparse 3D grid.
- **Conduction:** explicit diffusion with stable time steps.
- **Convection:** hot air rises, simplified as an upward transfer between air layers and
  advection by the wind field.
- **Radiation:**
  - Sunlight heats surfaces according to albedo, sun angle and cloud cover.
  - Clear nights cool the ground by radiation.
  - Fires and lava heat their surroundings within a radius.
- **Phase changes:** water freezes, ice melts, water evaporates and condenses, metals melt.
  These track latent heat, so melting a glacier with fire is expensive.
- **Body temperature:** creatures and the player exchange heat with the environment. This
  depends on air temperature, wind chill, wetness, insulation (fur, clothing, the cloak),
  exertion and shelter.

## 5.6 Fire and combustion

- A cell burns when it has **fuel**, **oxygen** and a **temperature at or above ignition**.
- Burning consumes fuel, releases heat (which ignites neighbours), produces smoke and
  CO₂, uses up oxygen, and leaves ash and char. The fuel load comes from vegetation
  biomass, dryness (moisture field) and material.
- **Spread** is driven by heat transfer and is strongly biased **downwind** and
  **uphill**. **Embers** are particles carried by the wind that can start spot fires
  across firebreaks.
- **Extinguishing:** rain, water spells, snow, lack of fuel (firebreaks, rivers, burned
  ground) and oxygen starvation inside closed rooms.
- **Aftermath:** burned ground becomes ash-rich, fertile soil and enters ecological
  succession (5.13). Burned buildings lose their timber pieces and may collapse.
- **Natural fires:** lightning from storms, volcanic activity and careless NPCs. Fires
  are normal in savanna and boreal forest.

## 5.7 Water and other liquids

| Water body | Model |
| --- | --- |
| **Oceans and large lakes** | Static level with visual waves; tides as a slow visual level change; currents as a velocity field from the global model |
| **Rivers** | Spline-based flow with depth and velocity profiles. Fast and stable, they push the carpet, swimmers and floating objects. In the active tier they couple with the shallow-water grid at their banks |
| **Local surface water** (active tier) | **Shallow-water simulation** on the terrain grid (pipe model): puddles, runoff, floods, breached dams, water summoned by spells, draining ponds. Mass is conserved |
| **Lava** | The same model with high viscosity, very high temperature and solidification into rock as it cools |
| **Snow and ice** | Snow depth accumulates with precipitation below freezing and melts with heat (meltwater feeds the fluid). Lake ice forms with sustained cold and is walkable above a certain thickness |
| **Ground moisture** | Per-cell soil moisture: rain soaks in, the ground dries with heat and wind, and plants draw water. This drives plant health and fire danger |
| **Groundwater** | Water-table depth from the planet model gives springs, wells and oases. It is not simulated dynamically |

Spells interact naturally: *Create Water* adds water mass to the shallow-water grid,
*Move Water* applies an impulse, *Shape Water* freezes cells into ice, and *Unmake Water*
evaporates them.

## 5.8 Air, wind and gases

- **Wind field:** the global weather gives the large-scale wind. Locally it is modified by
  terrain (speed-up over ridges, sheltering in valleys and forests, channelling through
  gaps) and by heat (thermals over hot ground, fire-driven inflow).
- **Gas concentrations** are advection-diffusion fields for **smoke, steam, toxic gases
  (volcanic vapours, alchemical fumes), spores, scents and oxygen** in enclosed spaces.
- **Scent:** each creature emits scent, which the wind carries. Predators track prey by
  smell, so the player should approach from downwind.
- **Breathing:** creatures and the player need oxygen. In closed rooms and caves, smoke
  and gases accumulate, and fire uses up oxygen. Ventilation through doors and windows
  is part of the interior grid.

## 5.9 Weather and seasons

- **Global:** pressure systems and fronts move with the prevailing winds over the macro
  grid. They carry clouds, precipitation and wind. Seasonal patterns come from the
  climate model, such as monsoons, dry seasons, winter storms and polar nights.
- **Local:**
  - clouds (sprite or volumetric impostors) and rain, snow, hail and fog,
  - thunderstorms with **lightning**, which strikes high points and ignites vegetation,
  - sandstorms and blizzards,
  - rainbows when sun and rain align.
- **Effects:**
  - **On flight:** wind, turbulence, visibility and a wet carpet.
  - **On fire:** wetness and rain.
  - **On creatures:** they shelter and change activity.
  - **On mana:** storms spike Storm mana, and snowfields boost Light mana.
  - **On plants:** water supply and frost damage.
  - **On survival:** body temperature and exposure.
- **Seasons:**
  - Vegetation follows the seasons: leaf color and fall, flowering and fruiting, dormancy
    and snow cover.
  - Animals migrate and hibernate, and breeding seasons bring young animals in spring.
  - Rivers rise with snowmelt.
  - Ritual timing depends on equinoxes and solstices.

## 5.10 Physics and destruction

- **Rigid bodies:** spheres, capsules and boxes. They collide with the heightfield,
  voxel volumes and building pieces. Everything uses simple impulse-based dynamics, with
  sleeping and broadphase grids per chunk.
- **Projectiles** (spells, thrown stones, arrows) are swept tests and are deterministic.
- **Buoyancy:** objects float according to density, and wood floats. The carpet hovers
  and isn't affected.
- **Terrain deformation:** explosions dig craters, and Earth spells raise or lower land.
  Both edit the heightfield, and voxel volumes are used when an overhang results. The
  edits are stored as deltas.
- **Structural integrity (heuristic):** buildings are made of kit pieces such as walls,
  floors, beams and roofs. When pieces are destroyed, by burning, by force or by Earth
  magic, a support check finds unsupported pieces. Those pieces fall as rigid debris.
  Castles can be breached, bridges can collapse and towers can topple.

## 5.11 Chemistry

### Model

- **Substances:** a registry of chemical substances such as water, ethanol, saltpeter,
  sulfur, charcoal, quicklime, copper salts, acids, plant extracts and toxins. Each has
  properties: phase, density, solubility, color, toxicity, taste and odour, nutritional
  value and mana affinity.
- **Mixtures:** containers (vessels, cells and creature stomachs) hold mixtures:
  substance amounts, temperature and phase partitions.
- **Reactions** form a data-driven table:
  - reactants and products in proportions,
  - conditions: temperature range, catalyst, medium, light and **mana of a color**,
  - an enthalpy (heat released or absorbed),
  - a rate law, simplified to a temperature curve in the style of Arrhenius, with
    concentration factors.
- **Processes** are the physical operations of the apparatus: heating at a controlled
  temperature, distillation (separation by boiling point), dissolution and filtration
  (by solubility), crystallization (by cooling and evaporation), grinding (which raises
  reaction rates), and fermentation (a biological reaction driven by yeast or bacteria).

### Where it runs

- **Laboratory vessels** use a precise per-vessel simulation. This is the alchemy
  minigame.
- **World cells** use coarse per-cell reactions for the important cases, such as
  quicklime with water (heat), acid on metal or stone, rusting, salting, smouldering
  peat, black-powder ignition and fermenting fruit.
- **Bodies:** ingested substances (food, potions, poisons) react according to their
  effect profiles, which give nutrition, healing, intoxication or poisoning.

### Example chain

Gather willow bark and an alcohol solvent, macerate them, distil, and concentrate the
result into a pain-relieving tincture. Add Life mana during the final reduction and it
becomes a healing draught.

## 5.12 Biology

### Plants

- **Species data:** growth form (tree, shrub, grass, herb, vine, cactus, moss, aquatic or
  epiphyte) and climate tolerances (temperature, moisture, light, soil type and
  pH). Plus life cycle and growth rates, seasonal behaviour, size, flammability,
  **edible and alchemical constituents** (substance links), mana affinity and sprites.
- **Placement:** an ecological suitability model computes the local density per species,
  including competition, so that dominant species shade others out. Instances are placed
  with weighted Poisson-disk sampling. A temperate climax forest has hundreds to
  thousands of trees per hectare, plus understory and ground cover.
- **Individuals versus statistics:** in the active tier, individual plants have state:
  growth stage, health, moisture stress, fruit, burnt or cut. Elsewhere, plants exist as
  deterministic placements plus per-chunk aggregate state. Only *changed* individuals are
  stored, for example a felled tree, a harvested herb or a sapling the player planted.
- **Harvesting** gives wood, bark, leaves, flowers, roots, seeds, fruit and resin,
  according to plant part and season. Knowing the right method from the Codex raises the
  yield.

### Fungi

- Fungi are decomposers. Mushrooms fruit on deadwood, dung and humus after rain in damp,
  shady places, and some fungi are seasonal (autumn flushes).
- **Mycelium** density is a per-chunk field that speeds up decomposition and feeds the
  Decay mana field.
- Fungi are important alchemy ingredients. They can be edible, medicinal, toxic or
  hallucinogenic, and visions count as a Void property. Giant fungi grow in Mirefen.

### Bacteria and microbes

Microbes are not individual entities. They are **rates and fields**:

- **Decomposition** depends on temperature, moisture and oxygen. It returns nutrients to
  the soil, which raises soil fertility.
- **Spoilage** of food: microbe load grows with time, warmth and moisture. Salting,
  drying, smoking, cooling, alchemical preservatives and the bag's slowed time all slow
  it down.
- **Fermentation:** bread, beer and wine, and vinegar. These are alchemy and cooking
  processes.
- **Disease:** contaminated water (swamps, near carrion) and spoiled food carry an
  infection risk. Wounds can get infected. Treatment comes from boiling water, herbal
  antiseptics, alchemy and Life magic. Some creature populations get epidemics, which
  is part of the population model.

### Insects and small life

- **Swarms** are particle-boid systems with behaviour: bees around flowers, mosquitoes
  over swamps at dusk, fireflies at summer nights, locusts in outbreaks, and butterflies
  in meadows.
- **Ecological fields:** pollination raises fruit yield and pest pressure damages crops
  and trees, both through fields.
- **Notable individuals** can be collected: beetles, glowing larvae and spiders (for
  silk). Some giant magical insects are full creatures.
- Insects are **ingredients and food** (honey and wax from bees, dyes from certain
  beetles), and they act as **disease vectors** (mosquitoes in swamps).

### Animals

- **Species data:**
  - size, diet (herbivore, carnivore, omnivore, scavenger, insectivore), habitat and
    biome preferences, activity cycle (diurnal, nocturnal, crepuscular), social
    structure (solitary, pair, herd, pack, flock, colony),
  - **senses:** sight with a light and contrast model, hearing and smell carried by the
    wind,
  - **needs:** hunger, thirst, rest, safety, reproduction and temperature,
  - migration and hibernation, and products (meat, hide, fur, feathers, bone, antler,
    venom and alchemical parts).
- **Behaviour:** utility-based AI scores actions against needs and threats (graze, hunt,
  drink, flee, rest, follow the herd, defend territory, migrate, care for young), with
  stimuli from perception. Herds and flocks use flocking. Predators stalk, chase and give
  up based on energy. Prey watches, flees and stampedes.
- **Interaction with the simulation:** animals avoid and flee fire, seek shade and water
  in heat, shelter in storms, follow seasonal food, and leave tracks, dung and scent.
- **Domestic animals** live in settlements: goats, sheep, cattle, chickens, dogs and
  horses.

### Magical creatures

Magical creatures are part of the ecology. They are **species adapted to extreme mana
fields**:

- salamanders in the Ashlands,
- storm birds over the Stormspires,
- crystal-backed deer in the Glasswood,
- floating jellyfish-like drifters and sky rays in the Hollows,
- giant fungal walkers in the Mirefen,
- griffons and wyverns as apex flyers of the highlands,
- sea serpents in the deep oceans.

They feed, hunt and reproduce like other animals, with the addition of a **mana
metabolism**. They draw from the field, and at death they release **mana motes**. Mythic
creatures are generic folklore archetypes with an original design.

## 5.13 Ecology dynamics

- **Populations per chunk:** each species has a count that is updated with logistic
  growth toward a carrying capacity, which depends on vegetation and prey. Predation uses
  Lotka-Volterra-style coupling. Migration flows to neighbouring chunks along suitability
  gradients.
- **Virtualisation:** when a chunk enters the active or near tier, individuals spawn from
  the counts, with deterministic identities per chunk and slot. When it leaves those
  tiers, individuals fold back into counts, except persistent ones such as named beasts,
  familiars and wounded quarry being tracked.
- **Succession:** a disturbance (fire, felling, flood, spell) resets a patch to an earlier
  stage. The stages are bare ground or ash, then grasses and herbs, then shrubs, then
  pioneer trees, then the climax community. Each patch progresses over in-game seasons
  to years. The player can speed this up with Life rituals.
- **Food web** is defined in species data (who eats whom). Overhunting a herbivore lets
  plants spread and starves predators, which may then come closer to villages and the
  player's tent.

## 5.14 Runaway guard rails

Emergent systems can run away, for example a single fireball burning down a continent.
The guard rails are:

- **Natural brakes:** fuel moisture, rivers and lakes, rain triggered by fire-induced
  convection, and burned firebreaks.
- **Spatial limits:** the full simulation only runs in the active tier. The near and far
  tiers use bounded front models. Dormant catch-up caps how far a fire spreads per day,
  based on fuel and weather.
- **Invariant tests:** mass conservation (water), energy bounds (heat), no NaN or Inf,
  and population counts that stay non-negative and below capacity. These are checked in
  automated tests and debug builds.

## 5.15 Performance budgets (targets, to be measured from Milestone 3)

| Budget item | Desktop mid-range | Web desktop | Mobile web |
| --- | --- | --- | --- |
| Simulation per 60 Hz tick (all threads, amortised) | ≤ 6 ms | ≤ 8 ms | ≤ 10 ms with smaller active radius |
| Active tier radius | 384 m | 256 m | 160 m |
| Simultaneous active creatures | 300 | 150 | 80 |
| Chunk generation (stages 1–3) on one worker | ≤ 8 ms | ≤ 16 ms | ≤ 25 ms |
| Resident memory (world data) | ≤ 1.5 GB | ≤ 800 MB | ≤ 400 MB |
