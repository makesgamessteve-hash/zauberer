# 6. Settlements, Architecture and People

## 6.1 Goals

- **Real places.** Every house, tower and castle can be entered. Interiors have a function
  and are furnished according to who lives there.
- **Generated in layers, lazily.** Settlements are planned at region level. Building shells
  are realised with the chunk, interiors when the player approaches, and furnishing when
  they get close or open the door.
- **Grounded.** Settlements are where people would really settle. Architecture follows
  climate, available materials and culture.

## 6.2 Where settlements come from

Settlements are not scattered at random. They are the present-day output of the world's
history (see [World history](10-world-history.md)), produced either by the *Settled
world* placement or by the *Rise and fall* simulation. Either way, every settlement
arrives with:

- **a site** scored for fresh water, fertile soil, defensibility (hilltops, river bends),
  resources (ore, salt, timber, fish) and trade (crossings, passes, oases, coasts),
- **a people and a faith:** culture, language, style family, majority faith and minorities,
- **a past:** founding date, rulers, wars, disasters, great works and, for ruins, the cause
  and date of their fall,
- **a present:** population, prosperity, realm, trade links and grudges.

The **ancient layer** of elder ruins at the major nexuses comes from the first mage
cultures (glyph-covered stone, vaults and gates in the Old Tongue). Region generation
turns these records into streets, buildings and rooms. The global simulation continues to
evolve them during play (6.12).

## 6.3 Settlement types

| Type | Typical size | Where |
| --- | --- | --- |
| Farmstead / hamlet | 1–8 buildings | Anywhere arable |
| Farming village | 15–60 buildings | Fertile plains, river valleys |
| Fishing village | 10–40 | Coasts, large lakes |
| Mining camp / town | 10–80 | Ore provinces, mountains |
| Market town | 60–250 | Crossroads, rivers |
| Oasis town | 20–150 | Desert springs, qanat outlets, wadis |
| Caravanserai | 1 fortified courtyard complex, 20–60 rooms | About a day's journey apart along caravan routes |
| Walled city with bazaar and citadel | 250–1,500 | Major rivers, oasis chains, coasts, realm capitals |
| Citadel or castle | 1 complex, 50–150 rooms | Borders, passes, hilltops |
| Temple complex or monastery (fictional faiths) | 10–40 | Remote heights, holy sites, nexuses |
| Mage tower | 1 tower plus outbuildings | Ley nexuses, isolated places |
| Nomad camp | 5–30 tents (moving) | Steppe, desert, tundra |
| Stilt village | 10–50 | Swamps, mangroves, floodplains |
| Cliff dwellings | 10–60 | Arid canyons |
| Ruins (of any of the above) | — | Wherever history left them |

## 6.4 Settlement layout

1. **Road skeleton:** entry roads from the region road network meet in a centre (square,
   market, crossing or harbour). Secondary streets follow the contours of the terrain.
2. **Plots** are split along the streets. Plot size depends on wealth and density.
3. **Building assignment by role:**
   - homes,
   - workshops by local trades (smithy, tannery near water and downwind, mill on the
     river),
   - inn or caravanserai, temples and shrines, bazaar, bathhouse, granary, wells, cisterns
     and covered water reservoirs, qanat outlets, and docks,
   - walls, gates and towers for larger settlements.
4. **Land use around the settlement:** fields with crops by climate, pastures, orchards,
   woodlots, fishing grounds and quarries.
5. **Terrain fit:** buildings are terraced on slopes, built on stilts over water, and set
   on plinths. Streets get stairs when they are steep.

## 6.5 Buildings: the generation pipeline

| Step | What happens | When |
| --- | --- | --- |
| 1. **Program** | The building type defines required and optional rooms and their relations. Example: farmhouse = hearth room, sleeping loft, pantry, attached byre | Region plan |
| 2. **Massing** | A shape grammar in the style of the culture produces the footprint, storeys, roof type (dome, vault, flat roof with windcatchers, gable, thatch), courtyards and iwans, wall material and openings | Chunk stage 4 (exterior shell) |
| 3. **Floor plan** | Rooms are placed by constrained subdivision of the footprint, with corridors for larger buildings. Doors follow the room adjacency graph, windows go on exterior walls (by style and climate), and stairs and ladders connect storeys | Player within ≈ 100 m, or interior visible |
| 4. **Construction** | The building is assembled from **modular kit pieces** on a 0.5 m grid: wall segments, floors, beams, roof tiles, stairs, doors, shutters and windows. Each piece has a material, so fire, force and decay act on individual pieces | Same as 3 |
| 5. **Furnishing** | Room-type rules place furniture: anchors against walls, clearances, functional groups (bed + chest, table + chairs, hearth + pot). Then clutter, light sources, containers and books, all matched to the **household** (profession, wealth, culture) | Player within ≈ 30 m, or opening the door |
| 6. **Contents** | Container contents, readable books (generated lore), personal items and food stores | On opening, or on inspection |

Each step is deterministic from the building's seed. Only changes are stored, such as
taken items, broken doors, burnt walls and moved furniture.

**Interiors are seamless.** There is no loading screen. Interiors live in the same world
space and use the same simulation: smoke fills rooms, fire spreads along timber, and doors
and windows control airflow and light.

## 6.6 Citadels and castles

Citadels and castles are the showpiece of procedural architecture.

- **Siting:** hilltop, ridge spur, river bend, lake island or coastal promontory. The
  curtain wall follows the terrain's defensible contour.
- **Program:**
  - curtain wall with wall-walks and crenellations, corner and flanking towers, and a
    gatehouse with gate, portcullis and murder holes,
  - optional moat or ditch, inner and outer bailey,
  - keep or palace core, audience hall with an iwan, the ruler's quarters, a garden court,
    a shrine of the realm's faith, kitchens, bakery, storerooms, bathhouse, cistern and
    well house, barracks, stables, smithy and armoury,
  - library, cellars, dungeon and secret passages.
- **Styles by culture** (from the style families, see
  [World history §10.4](10-world-history.md#104-phase-3-peopling-common-foundations)):
  highland citadels of mud brick and stone above oasis towns, kasbahs and fortified
  granaries in arid mountains, round-towered desert forts, stone fortresses in temperate
  uplands, brick river fortresses, steppe ring-forts and cliff-carved strongholds in
  canyons.
- **States:** held by a ruler with guards, servants and a daily life schedule, abandoned,
  ruined, occupied by bandits or a rival mage, or seized by Azhdar's followers.
- **Gameplay:** infiltration (use the cloak's camouflage, the guards' sight model, night),
  negotiation with rulers, sieges, which the player can break with terrain and fire
  magic, and libraries holding rare spellbooks.

## 6.7 Caves and dungeons

- **Natural caves** come from the region cave graph and depend on geology:
  - karst systems with stalactites, underground rivers and lakes,
  - lava tubes,
  - sea caves,
  - glacier caves.
  They are built from sparse voxel volumes, with a separate 3D simulation for air, water
  and heat.
- **Built dungeons** include crypts, catacombs, **tombs of notable figures** from the
  history database, abandoned mines, elder vaults, mage sanctums and bandit hideouts. They are generated as **graph grammars**: a mission graph
  with locks and keys, loops and optional branches is laid out spatially and then
  furnished with kit pieces.
- **Simulation-based puzzles and traps:**
  - melt an ice-sealed door,
  - burn away thorny vines,
  - flood a chamber to float up to a ledge,
  - redirect light into a crystal,
  - avoid pressure plates that release gas (gas fields!),
  - extinguish torches to sneak past.
- **Inhabitants** are cave-adapted creatures (bats, blind salamanders, fungal life,
  predators), bandits, rival mages and Azhdar's servants.

## 6.8 Ruins and decay

Any structure can be generated in a **ruined state**, parameterised by age and cause
(war, fire, abandonment, flood, magic):

- Roofs and upper floors are missing, walls are partially collapsed, and rubble mounds lie
  where they fell.
- **Vegetation overgrowth:** moss, ivy, saplings and trees growing through floors, using
  the same plant system.
- **Burial:** sand in deserts, silt in floodplains and snow in cold regions.
- Basements are flooded, and loot is sparse and scattered.

**Burnt villages** from Azhdar's campaign are ruins that appear *during play*. The global
simulation flips a settlement to "destroyed", and its chunks regenerate with fire damage
and no inhabitants.

## 6.9 Set pieces

Story locations use **parametric templates** written in Lua. Examples are Anarvan,
Farrokh's sanctum, the hermit's tower, the Circle's vaults and Azhdar's citadel. A template
fixes the essential layout and scripted elements and lets the procedural pipeline fill in
the rest (terrain fit, style, furnishing). The narrative director places them (see
[Story](01-story.md)).

## 6.10 People (NPCs)

- **Generated lazily:** a settlement knows its population and household count. Individual
  NPCs are generated when the settlement reaches the near tier: name (culture language),
  age, family, profession, wealth, faith, traits and relationships. NPCs become **persistent**
  once the player has interacted with them.
- **Daily life:** schedules for work, meals, sleep, worship, the teahouse and the bazaar,
  plus festivals from the culture's calendar.
  NPCs respond to weather, fire (they fight it with buckets), raids (they flee or defend)
  and night.
- **Perception of the player:** a reputation per settlement and faction, the cloak
  appearance (drab traveller or vivid mage), and magic they have seen (awe, fear,
  hostility, depending on culture and faith). Guest rights and hospitality customs
  matter: a guest who breaks them loses reputation fast.
- **Dialogue:** topic-based. Authored lines are combined with templated lines filled from
  world state and the history database: rumours about nearby events, directions, local
  history told from the culture's point of view, songs, grudges and prices.
- **Services:** trade, lodging, healing, teaching (herbalists, hunters, hedge mages and
  Circle remnants teach disciplines), and hiring guides.
- **Procedural requests** come from world state:
  - a predator is attacking the herds,
  - a child is lost in the forest,
  - the well is poisoned (a chemistry clue),
  - drought (rain ritual), wildfire approaching, or a bandit camp nearby.

## 6.11 Factions

| Faction | Attitude to the player | Role |
| --- | --- | --- |
| Realms and rulers (titles in the culture's language) | Varies, often wary of mages | Citadels, law, taxes, edicts on magic, sieges |
| Towns and guilds (herbalists, miners, merchants) | Pragmatic | Trade, teaching, materials |
| Faiths and temple orders | Depends on doctrine: revere, license or hunt mages | Temples, pilgrimages, healing, wards, inquisitorial orders |
| Free mages (rivals) | Varies by personality | Domains, competition, teaching, betrayal |
| The Ashen (Azhdar's followers) | Hostile | Raids, agents, lieutenants, the endgame |
| Bandits and raiders | Hostile | Camps, ambushes on roads |
| Nomads and forest folk | Neutral, can be won over | Guides, rare knowledge, beasts |

## 6.12 Economy and the living layer

- **Production:** settlements produce goods by their resources and trades. Prices depend
  on local supply and demand: salt is cheap on the coast and dear in the mountains.
  Barter is common, and coins are used in towns.
- **Caravans** travel the roads (a global simulation object) and appear physically when
  the player is near.
- **Change over time:** settlements grow, shrink, get raided, rebuild or get abandoned.
  The player's actions feed back: saving a village from a fire raises its prosperity, and
  clearing a bandit camp reopens trade.
