# peoples-and-cultures Specification

## Purpose
The Settled world: peoples, languages, faiths, realms, settlements and roads placed on the land, a chronicle written backwards from the present into the history database, the story woven in.

## Requirements

### Requirement: The land as peoples see it
The planet SHALL be sampled in regions of about 100 km with height, relief, climate, rivers, oases, fertility and neighbours, in created and noise worlds alike, deterministically; peopling SHALL take under 2 s on a desktop and run in slices so no tick exceeds the script budget.

#### Scenario: The land as peoples see it
- **WHEN** the engine's world is sampled
- **THEN** every region has neighbours, there are deserts, forests and steppes and rivers, and a region found by a position contains it

### Requirement: Peoples from cradles
Peoples SHALL settle cradles (fertile, well-watered regions, spaced apart) and spread by cheapest paths, rivers and coasts easier than deserts and mountains, each with a style family fitting its homeland, subsistence by its land, two values, an attitude to magic, a calendar and its crafts.

#### Scenario: Peoples from cradles
- **WHEN** the world is peopled
- **THEN** there are the founding peoples of several style families, each holding its cradle, and fewer barren deserts are settled than not

### Requirement: Related languages
Each language SHALL have its family's phonology and make names by kind (settlement, river, mountain, people, holy, realm, person); daughter languages SHALL share most roots by regular sound changes; words SHALL stay pronounceable.

#### Scenario: Related languages
- **WHEN** two daughters and a stranger are made and words named
- **THEN** the daughters are kin, share roots and differ from the stranger, names are stable and capitalised, and clusters are tidied

### Requirement: Faiths
Two to six faiths SHALL be founded at holy places with a concept, imagery from the homeland, a doctrine on magic, prayers, diet, burial, sacred animal and plant, and spread along easy ways, every settled region having a majority faith and some a minority.

#### Scenario: Faiths
- **WHEN** the faiths are read
- **THEN** each is complete with a holy site, and every settled region believes, some with a minority faith

### Requirement: Realms
Realms SHALL be carved around capitals, borders following rivers, each of a type (kingdom, empire, city-state, theocracy, merchant republic, nomad confederation, mage-ruled domain) with a title, laws and a stance on mages from its faith and people.

#### Scenario: Realms
- **WHEN** the realms are read
- **THEN** every settled region belongs to a realm, each realm has a capital among its places and a stance, and borders lie between neighbours

### Requirement: Present-day places
Settlements SHALL be sited by water, height and spacing, typed by their land and role (cities at capitals, oasis towns, temples at holy places, mage towers by nexuses, citadels on borders, nomad camps, stilt villages, fishing villages) and sized; roads SHALL join them, trade routes join capitals, caravanserais stand a day apart on dry routes.

#### Scenario: Present-day places
- **WHEN** the places are read
- **THEN** there are cities, villages and caravanserais on land, roads between them, and the nearest places are found in order

### Requirement: The history database
Entities SHALL have stable ids by kind; events SHALL have a year, kind, participants, place, cause and importance; minor events SHALL fold into summaries while events the present refers to and their causes stay; the database SHALL save and load.

#### Scenario: The history database
- **WHEN** events are added, pruned and saved
- **THEN** the referenced and important ones and their causes stay, the others fold into "border skirmishes, 900-950", and the database comes back

### Requirement: A reconstructed chronicle
The chronicle SHALL be written backwards from the present: settlings, foundings, dynasties and rulers with tombs, wars along today's borders won by the side holding the field, grudges, eruptions and floods, prophets, ruins with causes, elder ruins at nexuses, artifacts with provenance; no event after the present.

#### Scenario: A reconstructed chronicle
- **WHEN** the chronicle is read
- **THEN** every war's field is held by its winner and the loser keeps a grudge, every ruin has a cause, every tomb its ruler, and all is in the past

### Requirement: The story woven in
Anarvan SHALL lie where the carpet sets out, belonging to the nearest settled land; Farrokh its master weaver; the Circle of the Loom of seven broken forty years ago; Azhdar born in a real city; the story can be left out.

#### Scenario: The story woven in
- **WHEN** the chronicle is written with an anchor and without the story
- **THEN** Anarvan, Farrokh, the Circle and Azhdar are there as told, and without the story they are not

### Requirement: The same history for every heir
The same seed SHALL give every heir's game the same peoples, places and events (co-op: clients make it themselves; only changes travel); another seed another.

#### Scenario: The same history for every heir
- **WHEN** the world is peopled twice and once with another seed
- **THEN** the first two agree exactly and the third differs
