# survival Specification

## Purpose
Defines what the mage needs to stay alive: food, water, rest and warmth, the
conditions the land brings, sleep, and survival in the game.

## Requirements

### Requirement: Needs
Food and water SHALL drain a day's worth per game day at rest and faster with effort, cold, heat and sickness; rest SHALL drain over sixteen hours awake; empty food and water SHALL hurt; difficulty presets SHALL scale the drain (wanderer half, hermit more) and be saved.

#### Scenario: Needs
- **WHEN** a body rests and another works for six game hours, a starving one waits an hour, presets are compared, and a state is saved
- **THEN** the resting body lost a quarter of its food and water, the working one more and is more tired, starving hurts, nothing hurts when all is well, the wanderer drains slower and the hermit faster, and the saved state comes back

### Requirement: Body temperature
The core temperature SHALL settle towards what the air feels like (colder high up, in wind and wet; warmer in shelter, and in clothes and by a fire as far as the body wants: in the heat clothes are shed and a fire is sat back from); below 35.5 degrees it SHALL hurt by cold and above 39 by heat; rain and wading SHALL soak and a fire dry quickly.

#### Scenario: Body temperature
- **WHEN** bodies spend eight hours in a freezing wind with and without a fire and in desert heat working, feelings are compared, and a body is rained on, warmed by a fire and wades
- **THEN** the body without a fire freezes and is hurt by cold, the fire keeps it warmer, the heat overheats and hurts, a fire and clothes add no heat in warm air, wind and height feel colder, rain soaks, the fire dries, and wading soaks at once

### Requirement: Conditions
Sickness SHALL be caught with a chance from dirty water, spoilt or raw food and mosquito bites (none for the wanderer, none in the smoke of a fire close by), worsen for hours, drain water and rest, pass over about four days, and be treated and cured by remedies; poison SHALL hurt; salt water SHALL make one thirstier; food SHALL nourish by its calories.

#### Scenario: Conditions
- **WHEN** swamp water is drunk until sick, time passes, the sick drink herbal tea, a fly agaric, sea water, a meal and rotten meat are eaten, and the wanderer drinks swamp water
- **THEN** the body falls sick, worsens to its peak, drains faster, mends slowly untreated, is cured by two cups of tea, is poisoned and hurt by the mushroom, grows thirstier from sea water, is fed by the meal, mostly sickens from rotten meat, and the wanderer never falls sick

### Requirement: Sleep
Sleeping SHALL last up to the hours asked or until rested, about eight hours from exhausted, with food and water draining less, and the world's time SHALL move on by the hours slept.

#### Scenario: Sleep
- **WHEN** a tired body and a rested one sleep
- **THEN** the tired one sleeps about eight hours, is rested and hungrier but not starving, and the rested one does not sleep

### Requirement: Survival in the game
The game SHALL work out the needs every tick in game time from the weather, wind, ground, walker, fires close by and mosquito bites, deal their harms through the damage model, let the mage sleep on foot with the sleep action, show the needs and conditions on the HUD, and save them with the player.

#### Scenario: Survival in the game
- **WHEN** the game runs for twenty seconds, the mage steps off by a fire, sleeps tired, starves for a while, and the needs are saved
- **THEN** the HUD shows the needs, food drained by the game time passed, the fire is felt, the night is slept and the mage is rested, starving costs health and shows, and the saved needs come back

### Requirement: The M5 exit demo
The game SHALL let the mage live seven in-game days off the land in two contrasting biomes of a created world by foraging, hunting and cooking; swamp water drunk unboiled SHALL be able to make them sick and boiled water and herbal remedies cure them; and a predator downwind SHALL follow the mage's scent while one upwind does not.

#### Scenario: The M5 exit demo
- **WHEN** in a fair week the mage camps by a river in a cloud forest and then in the estuary below, hunts with Fireball, forages, lights campfires with Kindle, cooks, boils water, eats, drinks and sleeps by the fire for seven days, one evening a breeze carries their scent between two hungry wolves, and in the estuary they drink the river unboiled
- **THEN** a boar is hunted and butchered, the fires burn, the downwind wolf tracks the scent upwind and the upwind one never comes, the swamp water makes the mage sick, bark tea and boiled water cure them within two days, and after seven days they are alive, fed, watered and well with the land's life studied in the Codex
