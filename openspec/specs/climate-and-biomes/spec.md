# climate-and-biomes Specification

## Purpose
Defines the living planet's present: monthly climate from sun, sea, wind and relief;
rivers, lakes, ice and groundwater; biomes including magical ones; and the
biogeographic provinces that species will live in.

## Requirements

### Requirement: Temperature
Monthly temperature SHALL follow daily mean insolation for the axial tilt, with poleward heat transport, seasons that lag and swing more over land far from the sea than over the ocean, a lapse rate of 6.5 °C per km, and ocean currents that cool subtropical west coasts and warm east coasts.

#### Scenario: Insolation and seasons
- **WHEN** an ocean planet with a 23° tilt and one without tilt are computed
- **THEN** insolation is highest at the equator over the year with polar night and midnight sun, the equator averages 24–30 °C and is warmer than 45° and the poles, hemispheres have opposite seasons, and without tilt January equals July

#### Scenario: Continents swing, oceans moderate
- **WHEN** a large continent is computed
- **THEN** the seasonal range at 45° is largest inland, smaller on the coast and smallest at sea

#### Scenario: Altitude cools
- **WHEN** the temperature is read on a 3 km plateau and at sea level
- **THEN** the plateau is 19.5 °C colder

### Requirement: Precipitation
Monthly precipitation SHALL come from moisture that prevailing trades, westerlies and polar easterlies carry from evaporating oceans, raining out most in the intertropical convergence and where air is lifted over relief, least under the subtropical highs, in sinking lee air and from cold seas.

#### Scenario: Rain shadows
- **WHEN** a north-south range stands in the westerlies at 45° N
- **THEN** its windward side receives over one and a half times the rain of its lee

#### Scenario: Belts of rain and drought
- **WHEN** a continent spans the tropics and subtropics
- **THEN** the equator receives over twice the rain of 24° N, and the subtropical west coast is cooler and drier than the east coast

### Requirement: Hydrology
Runoff from rain and evaporation SHALL accumulate downstream into rivers (over 8 m³/s) with Strahler orders and mouths at the sea; lakes that evaporate all their inflow SHALL become salt lakes; cold land SHALL hold glaciers and cold seas sea ice; every land cell SHALL have a groundwater depth, shallow beside water, with oases where desert groundwater is shallow.

#### Scenario: Rivers and lakes
- **WHEN** a world's water is routed
- **THEN** discharge grows downstream outside lakes, river cells carry at least 8 m³/s with an order that never drops downstream, mouths meet the sea, salt lakes are lakes, and a world has many rivers with orders of three or more

#### Scenario: Ice and groundwater
- **WHEN** ice and groundwater are read
- **THEN** glaciers lie where it is colder than 2 °C, rivers and lakes have groundwater at 1 m, oases are deserts with groundwater shallower than 25 m, and some glaciers exist

### Requirement: Biomes
Every macro cell SHALL get one of the concept's biomes: aquatic ones at sea, lakes and great rivers; coastal ones on coasts; mountain biomes above 1,200 m; wetlands on flat lowland; volcanic slopes at active volcanoes; salt flats over geological salt in dry basins; and otherwise a Whittaker biome from annual temperature, precipitation, the warmest month and the dry season.

#### Scenario: Whittaker biomes
- **WHEN** the Whittaker rule is asked for eighteen typical climates
- **THEN** it returns the expected polar desert, ice sheet, tundra, taiga, deserts, steppe, shrubland, mediterranean scrub, forests, savanna, seasonal forest and rainforest, and every biome has a name and one of nine groups

#### Scenario: Biomes everywhere
- **WHEN** a world's biomes are classified
- **THEN** tropical, arid, temperate and cold biomes each cover a share of the land, the deep ocean exists, and ocean cells hold only ocean biomes

#### Scenario: Coasts, mountains and wetlands
- **WHEN** a world's biomes are checked cell by cell
- **THEN** coastal biomes lie on coasts, alpine, scree, montane and cloud forest above 1,200 m, wetlands below 250 m, and lake biomes on lakes

### Requirement: Magical biomes
Ley nexuses of strength at least 0.75 on land SHALL turn the land within 15–60 km (by strength) into a magical biome chosen by setting: Ashlands at volcanoes, the Hollows at the strongest triple junctions, Mirefen or Drowned Vale in wetlands, Wildheart in rainforest, Stormspires on arid highland, Glasswood over mana crystals, Deepstone on sutures and high ground.

#### Scenario: Magical biomes
- **WHEN** four nexuses are given: a strongest triple junction, a strong volcano, a weak suture and one at sea
- **THEN** the first two make the Hollows and Ashlands within their radius, the weak and the sea nexus make none, and land far away keeps its natural biome

### Requirement: Biogeographic provinces
All land SHALL be divided into provinces grown from seeds on every sizeable crust lineage of each large landmass, across narrow straits, slowed by high mountains, deserts and ice and stopped at lineage borders unless the lineages met within 50 Myr; remote islands SHALL form their own; each province SHALL have an isolation and every pair an affinity.

#### Scenario: Biogeographic provinces
- **WHEN** a world's provinces are read
- **THEN** every land cell belongs to one and no ocean cell does, their areas sum to the land, remote islands are more isolated than continents, affinity is full within a province and higher on one landmass than across the sea, and over four fifths of cells lie on their province's lineage or one that met it recently

### Requirement: Deterministic and bounded climate
Climate SHALL be a pure function of its input with the same hash on every platform, advance in bounded pieces (the grid, each month, hydrology, biomes) with progress, reject invalid input, and finish within 15 s for a default world on a desktop.

#### Scenario: Deterministic climate
- **WHEN** the same small world's climate is computed twice, natively and in WebAssembly
- **THEN** both give the recorded golden hash

#### Scenario: Climate time budget
- **WHEN** a default world's climate is computed in a release build
- **THEN** it finishes within 15 s
