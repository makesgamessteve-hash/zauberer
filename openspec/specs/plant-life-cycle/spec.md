# plant-life-cycle Specification

## Purpose
Defines the life of plants: life cycles and items as data, plants through the local
seasons, the parts that can be taken from them and their regrowth.

## Requirements

### Requirement: Life cycles and items
Every flora species SHALL have a life cycle as data: when it is in leaf, in flower and in fruit in seasons of the year, and its parts with the item each gives, when it can be taken, its yield for the smallest and the largest plant, its regrowth in game days, or that taking it takes the whole plant. Items SHALL be data with an id, a name, a kind (food, herb, material, water) and a weight. Bad data SHALL be refused with a message naming what is wrong.

#### Scenario: Life cycles and items are data
- **WHEN** the base items and life cycles are read, and broken ones are checked
- **THEN** every flora species has a life cycle whose parts give known items, and each broken one is refused with its reason

### Requirement: Plants through the seasons
A plant SHALL be in leaf, in flower, in fruit or bare by its local season (spring half a year apart across the equator), each plant up to about a week earlier or later than its species by its variant; windows SHALL run across the new year.

#### Scenario: Plants through the seasons
- **WHEN** oaks, hazels and spruces are looked at through the year, and an early and a late oak near the start of autumn
- **THEN** the oak flowers in spring, fruits in autumn and is bare in winter, the hazel flowers across the new year, the spruce keeps its needles, and the early oak fruits before the late one

### Requirement: Taking parts of plants
A plant SHALL offer the parts its stage allows, a count by its size that is the same within a year and differs between years; a part taken SHALL not be offered by the same plant until it has regrown; a whole part SHALL cut the plant; what was taken SHALL be saved with the game for as long as it matters.

#### Scenario: Taking parts of plants
- **WHEN** acorns, bark and wood are looked for on large and small oaks through the year, acorns are taken twice, a year passes, and the record is saved, restored and saved long after
- **THEN** acorns come only in autumn and more from a large oak, the second take is refused until a year has passed, counts repeat within a year and differ between years, the restored record still refuses, and the old record is dropped

#### Scenario: Plants from Lua
- **WHEN** the plants by the carpet are listed with their stage and offers, ground cover and a tree are taken whole, and three game weeks pass
- **THEN** every plant has its own id, size, variant and stage, both are gone at once, cutting them again or a plant that does not exist fails, the ground cover is back after three weeks and the tree is not
