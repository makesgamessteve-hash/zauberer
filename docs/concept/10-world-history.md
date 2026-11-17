# 10. World History: From Deep Time to the Present

Before the first frame of play, the planet has lived through its whole past. Continents
drifted, mountains rose and wore down, seas came and went, and ice ages carved valleys.
Then people arrived, spread, founded cities, worshipped, traded, fought and fell. The
present-day world the player explores is the **result** of that past, not a collage of
random features. Every mountain range, salt flat, ruin, language and faith has a cause
that can be traced back.

## 10.1 The creation pipeline

```
 Phase 1  DEEP TIME             plate tectonics, erosion, sea level, ice ages, volcanism
          (always)              → Earth-like planet, rock record, minerals, ley network
              │
 Phase 2  LIVING PLANET         present climate, biomes, biogeographic provinces,
          (always)              species lineages, fossils, magical biomes
              │
 Phase 3  PEOPLING              ┌─ Variant A "Settled world" (default, fast)
          (choose one)          │    direct placement of peoples + reconstructed chronicle
                                └─ Variant B "Rise and fall" (optional, deep)
                                     simulated societies over thousands of years
              │
 Phase 4  PRESENT DAY           settlements, roads, realms, ruins, holy sites,
          (always)              lore index, story backstory woven in
```

Both peopling variants write the **same output format** (10.11). Everything downstream
(region generation, NPCs, books, quests, the narrative director) works the same whichever
variant created the world. Variant B just produces a deeper, more surprising history.

## 10.2 Phase 1: Deep time (geological history)

### Model

- **Resolution:** the sphere is sampled with 40,000–160,000 points (a Fibonacci lattice)
  for the tectonic simulation. This is coarser than the 6 km macro grid, which is derived
  at the end.
- **Time span:** the last ≈ 500 million years, in 100–250 steps of 2–5 million years.
  Earlier history is summarised as old continental cores (*cratons*) present at the start.
- **State per point:** plate, crust type (continental or oceanic), thickness, age,
  elevation, orogeny history and the **rock record** (strata laid down so far).
- **Method:** in the spirit of published procedural tectonics work (e.g. *Procedural
  Tectonic Planets*, Cortial et al., 2019), simplified for speed. Plates rotate about
  Euler poles, and interactions at their boundaries produce geological features.

### Processes per step

| Process | Effect | Leaves behind |
| --- | --- | --- |
| **Subduction** (oceanic under continental or oceanic) | Trenches, volcanic arcs, uplift | Arc volcanoes, copper and gold belts, earthquake zones |
| **Continental collision** | Fold mountains; plates weld together | High ranges, metamorphic belts, old sutures (later ley lines) |
| **Rifting and spreading** | Continents split; new ocean floor | Rift valleys, rift lakes, young seas, basalt plateaus |
| **Supercontinent cycle** | Large plates trap heat until they rift apart | Alternating eras of one landmass and many |
| **Hotspots** (fixed mantle plumes) | Island chains, flood basalts | Volcanic islands in rows, lava plateaus |
| **Erosion and isostasy** | Mountains wear down, crust rebounds | Old rounded ranges next to young jagged ones |
| **Sedimentation** | Rivers fill basins, seas deposit on shelves | Sandstone, shale, limestone (later karst), coal from swamp eras, salt from dried seas |
| **Sea-level change** | Driven by ice volume and ridge activity | Marine terraces, inland seas that came and went, raised beaches |
| **Ice ages** | Triggered when continents sit near the poles and climate cools | Fjords, U-valleys, moraines, lake districts, loess plains |
| **Climate eras** | Arid and humid periods per region | Evaporite basins, red-bed deserts, **dry riverbeds (wadis)** in today's deserts, buried aquifers for oases and qanats |

### Rock record and resources

- Each cell keeps a compressed **stratigraphic column**: the top layers with rock type,
  thickness, age and origin ("marine limestone, 140 Myr, warm shallow sea"), on top of a
  basement type.
- **Minerals form by geological event**, not by noise:
  - copper and gold in arcs,
  - tin in granite belts,
  - iron in ancient banded formations,
  - salt and gypsum in evaporite basins,
  - **naphtha and bitumen seeps** in sedimentary basins (alchemy: incendiaries, mortar,
    waterproofing),
  - coal from swamp eras,
  - gemstones such as lapis lazuli, turquoise and carnelian in metamorphic belts,
  - mana crystals at old sutures and nexuses.
- **Visible history:** cliff faces and canyon walls render the real strata as bands of
  layered rock. Digging follows the column.
- **Fossils** form in sedimentary strata of the right ages: ancient sea creatures in
  limestone, giant beasts in river deposits, and amber in old forest strata. They are
  collectable, recorded in the Codex, used as alchemical ingredients, and show up in
  folklore.

### Earth-likeness steering

The user-facing promise is that the planet **settles into something similar to Earth**.
At the end of Phase 1 the result is checked against target metrics:

- land fraction 25–40 %,
- 3–8 continents larger than 1 % of the surface,
- at least one major mountain belt on every large continent,
- a mix of young and old ranges,
- ice caps under 15 % of the surface,
- every major climate zone represented.

Sea level is fitted to the land target. If other metrics miss, the final phase re-runs
deterministically with a derived sub-seed, with a bounded number of retries. World
options can deliberately select non-Earth targets: *Pangaea*, *Archipelago*, *Ice world*
and *Hothouse*.

### Amplification to the macro grid

The final tectonic state is resampled onto the 6 km macro grid. Feature-aware detail is
added: ridgelines along orogens, rift escarpments, and volcanic cones at arc points. Then
the macro erosion pass described in [World generation](04-world-generation.md) runs.

### The magical deep past

- **Ley lines** form along long-lived geological structures: old sutures, rifts and
  hotspot tracks. A ley line is literally a scar of the planet's history.
- **The Age of Mana.** Mana was far stronger in the deep past. Magical lineages of
  creatures thrived and left fossils with a residual glow (*mana fossils*). The slow
  decline of the ley network over geological time continues into the present, and this
  is the root of the story's central conflict.
- **Elder sites** are where the oldest nexuses formed. Phase 3 places the ruins of the
  first mage cultures there.

## 10.3 Phase 2: The living planet

- **Present climate** is computed from the final geography, including winds, currents,
  rain shadows and seasons (see [World generation](04-world-generation.md)). Biomes
  follow.
- **Biogeography:** the tectonic history records which landmasses were connected and for
  how long. Each landmass gets **biogeographic provinces** with their own species
  lineages, chosen from the species library by climate. Long-isolated continents and
  islands have more **endemic** species. Recently joined continents share fauna, while
  separated ones diverged. Two deserts on different continents therefore have different
  animals adapted to the same niche.
- **Magical biomes** appear where today's mana field is extreme (see
  [Magic](03-magic.md)).

## 10.4 Phase 3: Peopling (common foundations)

### Cradles

People first settle in **cradle regions** with reliable water, fertile soil and mild
climate. These are great river valleys, oasis chains and well-watered foothills, much
like Earth's historic cradles of civilisation.

### Style families: the Oriental setting

The game's setting is consciously **Oriental**: the world of the *Thousand and One Nights*
and the Silk Road, where flying carpets belong. Cultures draw on the long, rich
traditions of **Persia, Arabia, Anatolia, the Caucasus, Central Asia, North Africa and the
Indian subcontinent**. Every generated culture inherits one or two **style families**,
which are libraries of artistic and material vocabulary:

| Style family (inspiration) | Typical homeland in the generator | Vocabulary |
| --- | --- | --- |
| **Persianate highland** | Semi-arid plateaus and foothills | Domed halls, iwans, tiled portals, walled four-part gardens, windcatchers, qanat irrigation, pile carpets, miniature painting |
| **Arabian desert and coast** | Hot deserts, oases, coasts | Mud-brick tower houses, desert forts, souks, palm groves, falaj channels, dhow harbours, incense trade |
| **Anatolian–Caucasian upland** | Temperate highlands | Stone fortresses, carved wooden balconies, kilims, terraced villages, hot springs |
| **Steppe and Silk Road** | Grasslands, cold deserts | Felt yurts, horse herds, burial mounds, caravanserais, oasis cities with blue domes |
| **Maghreb** | Arid mountains and desert edges | Kasbahs, fortified granaries, rammed-earth villages, geometric tile and plaster |
| **Indian subcontinent** | Monsoon plains, tropical forest | Stepwells, palaces with latticed balconies, river ghats, cotton and indigo, temple-like halls (fictional faiths) |
| **Far East and periphery** (for variety at the world's edges) | Distant lands | Kept for contrast and to make travel feel vast |

Architecture grammars, ornament motifs (geometric star patterns, arabesque, floral
scrolls, calligraphic bands in invented scripts), palettes (lapis, turquoise, saffron,
pomegranate, ochre), textile and carpet patterns, clothing silhouettes, instruments,
cuisine and naming phonology all come from these families. Cultures blend and drift away
from their families over history, so no two cultures are identical.

> **Cultural respect.** The inspiration is real history and craft, not caricature. Peoples
> and faiths in the game are fictional; no real religion is depicted. There are no
> villain stereotypes tied to an ethnicity, and no "exotic" clichés in music or
> costume. Each style family is built from documented sources (architecture, textiles,
> music, cuisine), and those references are listed in the design notes.

## 10.5 Variant A: Settled world (default)

This variant produces a plausible, Earth-like present in seconds, without simulating
history year by year.

1. **Peoples** are seeded in the cradles and spread by **suitability flood fill**: they
   follow rivers and coasts, avoid deserts and high mountains, and adapt (herders on
   steppes, oasis farmers in deserts, fishers on coasts).
2. **Language families** are formed by spatial clustering of peoples, with related
   languages next to each other (10.8).
3. **Faiths** are placed with a core region and spread along trade corridors (10.9).
4. **Realms** are carved by cost-distance partitioning from capitals. Borders follow
   rivers and ridges.
5. **Settlements** are placed by site scoring (water, soil, defence, trade, resources).
   Sizes come from the carrying capacity of their surroundings.
6. **A reconstructed chronicle** is generated *backwards from the present*: eras,
   dynasties, founding dates, a set of wars along today's borders, migrations, disasters
   taken from the geological record (eruptions, earthquakes), and the origins of every
   ruin.

The chronicle is consistent with the map, but shallower and more predictable than
Variant B.

## 10.6 Variant B: Rise and fall (optional history simulation)

This variant simulates societies from the first farming villages to the present.

### Setup

- **Spatial units:** land is clustered into **provinces** of 1,000–3,000 km² each,
  roughly 6,000–17,000 on a default planet. Each has a carrying capacity (climate, soil,
  water, irrigation potential), resources, movement costs and natural defences.
- **Time:** 4,000 years by default (configurable 1,000–10,000). Events are resolved
  yearly, and demographics are updated every 5 years.
- **Technology ceiling:** pre-industrial. The present corresponds roughly to a classical
  or medieval Silk Road era. There are no firearms. Black powder exists only as an
  alchemical curiosity and as fireworks, and magic fills the niche of great destructive
  power.

### Actors

| Actor | Attributes |
| --- | --- |
| **Peoples / cultures** | Language, style family, values, subsistence (farming, herding, nomadism, trade, fishing), attitude to magic |
| **Settlements** | Population, wealth, walls, temples, workshops, founding date, ruler |
| **Polities** | Type (tribe, chiefdom, city-state, kingdom, empire, nomad confederation, theocracy, merchant republic, mage-ruled domain), territory, capital, dynasty, laws, stance on mages |
| **Faiths** | Doctrine, deities or principles, holy sites, clergy, festivals, attitude to magic |
| **Notable figures** | Rulers, prophets, generals, scholars, poets, master weavers, mages, with lives, deeds, relationships and burial sites |
| **Trade routes** | Caravan routes across deserts and steppes, maritime routes following seasonal winds, river routes |
| **Mage orders** | Circles, schools, towers on nexuses, rivalries, artifacts made and lost |

### Processes

| Process | Rules (heuristic) | Visible in the present |
| --- | --- | --- |
| **Demography** | Logistic growth toward carrying capacity; famines from droughts and floods; plagues spreading along trade routes | Abandoned villages, mass graves, Decay hotspots |
| **Agriculture and technology diffusion** | Irrigation (canals, qanats), terracing, crops and animals spread along contacts | Terraced hills, qanat lines across deserts, old canals |
| **Trade** | Flows between complementary resources; caravanserais and ports grow on routes | Caravanserais every day's journey, wealthy route cities, dead cities on abandoned routes |
| **Ethnogenesis and drift** | Large or isolated populations split; languages and styles drift; contact blends them | Language families, dialect chains, hybrid styles in border zones |
| **Faith founding and spread** | Prophets arise at crises or sacred places; faiths spread by trade, conquest and preaching; schisms on doctrine or politics | Holy sites, pilgrim roads, ruined temples of extinct faiths |
| **State formation** | Chiefdoms consolidate into states when surplus and population allow; dynasties found and end | Capitals, palaces, dynastic tombs |
| **Diplomacy** | Alliances, marriages, tribute, vassalage | Treaty stones, border forts, mixed dynasties |
| **War** | Casus belli (land, succession, faith, revenge, raids), campaigns along movement costs, battles resolved statistically (numbers, terrain, leadership, magic), sieges, sacks | Battlefields (Decay hotspots, relics), ruined cities, border castles, refugee settlements |
| **Nomad incursions** | Steppe confederations raid or conquer settled lands in bad years | Defensive walls, fortified caravanserais, conquest dynasties |
| **Collapse** | Overextension, succession crises, plague, climate shifts, invasion | Fragmented successor states, ruins of empires |
| **Great works** | Rich, stable polities build monuments, libraries, observatories, bridges, canals and gardens | Landmarks, libraries holding lore and spellbooks |
| **Magic in history** | Mage orders found towers on nexuses; mage wars leave blight and scars; artifacts are created, lost and buried; rulers employ or persecute mages | Blighted lands, tower ruins, lost artifacts in tombs, laws against magic |
| **Natural disasters** | Eruptions and earthquakes where Phase 1 put active boundaries; floods on great rivers; droughts in arid eras | Buried towns, moved riverbeds, legends of divine wrath |

### Output

The present-day world plus a **history database** (10.11). A typical default run produces
tens of thousands of events, of which a few thousand are kept at full detail (see
pruning in 10.11).

## 10.7 Cultures

Each culture is a record that drives generation, NPCs and lore:

- **Identity:** name (in its own language), language, style families and blend weights,
  homeland and diaspora.
- **Material life:** subsistence, cuisine (from local crops and animals; recipes become
  cooking and alchemy knowledge), crafts and trade goods, **carpet and textile pattern
  vocabulary** (the cloak can learn patterns), clothing, music and instruments.
- **Social life:** social structure, values (hospitality, honour, learning, piety,
  martial prowess, commerce), gender and family customs (the player's chosen protagonist
  is accepted equally everywhere), hospitality customs (guest rights), etiquette that
  NPCs expect.
- **Calendar:** a solar, lunar or mixed calendar, festivals tied to seasons, harvests and
  astronomy, and historical commemorations.
- **Attitude to magic:** revered, regulated (licensed mages), feared, or forbidden. This
  drives NPC reactions to the cloak and to visible casting.
- **Memory:** heroes, villains, grievances against neighbours and founding myths, drawn
  from the history database.

## 10.8 Languages

- Each language has a **phonology** (sound inventory, syllable patterns), simple
  **morphology** for names (compounds, suffixes for places, such as "-abad" for a
  settlement, "-rud" for a river, "-kuh" for a mountain), and a **lexicon** of a few
  hundred roots for names and inscriptions.
- **Drift and families:** when peoples split, their languages copy the parent and apply
  sound changes over time. Related languages share recognisable roots, so place names
  across a region feel coherent, and old ruins carry names in ancestral languages.
- **Scripts:** each family has an invented script, drawn as a pixel glyph set, for
  inscriptions, books and signage. A rare **knowledge option**: some ancient texts can
  only be read after learning the script. This counts as Naturalism (lore) study.
- The **Old Tongue** of magic (see [Magic](03-magic.md)) is the language of the elder
  mage culture, and the ancestor of several living scholarly languages.

## 10.9 Religions

Faiths are generated, never copied from real religions.

| Aspect | Generated from |
| --- | --- |
| **Core concept** | Pantheon, single deity, dual principles, ancestor veneration, philosophy without gods, or veneration of a mana color |
| **Sacred imagery** | The homeland's environment and history: a river people's water deity, a desert faith of stars and the sun, a volcano remembered as a wrathful god |
| **Founding myth** | Real events in the history database, retold mythically (an eruption becomes divine punishment; a mage-hero becomes a saint or a demon) |
| **Doctrine** | Ethics, afterlife, the status of magic (sacred gift, dangerous power to be licensed, or abomination), relations to other faiths |
| **Practice** | Prayer times, festivals, pilgrimages, dietary rules, burial customs (tombs, cremation, sky or water burial), sacred animals and plants |
| **Institutions** | Clergy, temple orders, monasteries, mendicants, inquisitorial orders that hunt rogue mages |
| **Holy sites** | Springs, peaks, caves, battlefields and **nexuses**. Rituals at holy sites resonate, with bonuses or penalties by color |

Faiths spread, split into schisms and heresies, merge with local cults, decline and die.
Dead faiths leave ruined temples, forgotten gods' names in place names, and relics.

**Gameplay effects:**

- NPC attitudes depend on the faith's doctrine on magic.
- Hunting sacred animals or harvesting sacred plants affects reputation.
- Pilgrim caravans travel the roads, and festivals happen on calendar dates.
- Temples offer healing, lore or refuge, and temple orders form factions (see
  [Settlements and architecture](06-settlements-architecture.md)).

## 10.10 Polities and notable figures

- **Polities** have territory, capital, dynasty, laws, vassals, enemies, wealth and an
  official faith. In the present, they hold castles and cities, and they post guards,
  tax collectors and edicts about magic.
- **Notable figures** have a biography: birth, deeds, relationships, death and burial
  place. Their **tombs** are dungeon candidates. Statues, inscriptions, songs, portraits
  in miniature-painting style, and heirlooms with **provenance** (who made, owned and
  lost them) make history tangible.

## 10.11 The history database and lore

Both variants write one schema:

- **Entities:** cultures, languages, faiths, polities, dynasties, settlements, figures,
  artifacts and sites, each with a stable ID.
- **Events:** time, place, participants, cause (a link to an earlier event), outcome and
  importance.
- **Present-state tables:** the world as it is today.
- **Pruning:** minor events are aggregated ("border skirmishes, 1120–1180"). Events that
  are referenced by the present (ruins, tombs, names, grudges) are always kept.

**Lore generated from the database:**

- **Chronicles and Books of Kings** per dynasty. They are **told from a perspective**: the
  same war reads as a glorious victory in one culture's chronicle and a treacherous
  massacre in another's. The player can piece together the truth.
- **Legends and folk tales** that distort real events with time and distance.
- **Inscriptions** on monuments, waystones and tombs, in the culture's script.
- **Place names** derived from events and figures ("Field of the Seven Banners", "the
  Weeping Bridge").
- **NPC knowledge:** NPCs know the history of their region and faith, biased by culture,
  and they share rumours, songs and grudges.
- **Codex sections:** *Peoples*, *Faiths*, *Realms* and *History*. They are unlocked
  through books, conversation and visiting sites, never all at once.

## 10.12 Story integration

The narrative director (see [Story](01-story.md)) **weaves the authored backstory into
the generated history**:

- The **Circle of the Loom** is placed about 40 years before the present. Its seven
  members are given homelands and families in suitable generated cultures. The Great
  Weaves' creation, the Circle's breaking and the hiding of the Weaves become events in
  the database, with sites (the Circle's meeting hall, a member's tomb) reserved for
  story beats.
- **Azhdar's** origin, rise and early crimes are anchored to a real polity and city, so
  history books and NPCs in that region know (and distort) his past.
- **Farrokh's** life as the master weaver of Anarvan, and before that as the Circle's
  youngest member, is written into the local chronicle. Old villagers' tales in the
  region hint at it before the player learns the truth.
- The **ley decline** recorded in deep time is reflected in history: mage orders that
  once flourished and are now gone, and the shrinking of magical biomes in recent
  centuries.

## 10.13 The world-creation experience

- **Time-lapse globe:** while Phase 1 runs, the player watches continents drift, ranges
  rise and ice advance on a rotating retro-rendered globe, with an era counter. During
  Variant B, polity borders, trade routes and faiths spread and wane on the map, with key
  events captioned ("The Empire of Kashvar falls to steppe riders").
- **Options:** history variant (A or B), history length, number of founding peoples,
  conflict intensity, magic prevalence in history, religious diversity, and an Earth-like
  or exotic geology target.
- **Spoiler policy:** by default the player only sees the time-lapse. A **World Atlas and
  Chronicle** viewer (all entities and events) is available for curious players and mod
  authors, but is off by default. In normal play, history is discovered.

## 10.14 Budgets, determinism and storage

| Step | Desktop (default settings) | Web desktop | Mobile web |
| --- | --- | --- | --- |
| Phase 1 deep time | ≤ 60 s | ≤ 2 min | "Quick" quality ≤ 2 min |
| Phases 2 + 3A + 4 | ≤ 15 s | ≤ 30 s | ≤ 45 s |
| Variant B (adds) | ≤ 3 min | ≤ 6 min | Not recommended; offered with a warning |

- Creation runs on the job system with a progress display, and it can be cancelled.
- **Determinism:** every phase is deterministic from the seed and options, using the
  rules in [World generation §4.8](04-world-generation.md#48-determinism-and-versioning).
  Golden-hash tests run reduced-size worlds on every platform.
- **Stored, not recomputed:** because these simulations are long and chaotic, their
  **results are saved with the world**. This is the *world bundle*: macro-grid fields,
  rock-record dictionary, ley graph and history database, typically 10–30 MB
  compressed. Loading a save never re-runs history, so saves stay valid even if a future
  version tunes the simulation. Everything below the planet layer (regions, chunks,
  rooms) is still generated lazily from the bundle and the seed.
- **Shareable worlds:** a seed plus options recreates the same world on any platform, and
  a world bundle can also be exported and shared directly.
