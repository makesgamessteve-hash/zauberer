# fauna-behaviour Specification

## Purpose
Defines how animals live by their needs and the day: drinking and resting, predators
hunting prey, herds and stampedes, scent trails followed upwind, and fauna in the game.

## Requirements

### Requirement: Needs and the day
Thirst SHALL grow while a creature is about; a thirsty creature SHALL find open water nearby, walk to its edge and drink. Each species SHALL be about by day, by night, from dusk to dawn or at any hour, and rest the rest of the time, but not with a predator in view.

#### Scenario: Thirst and the time of day
- **WHEN** a thirsty deer stands near a pond, needs are set for a creature that does not exist, and a night animal is looked at at noon and at midnight
- **THEN** the deer walks to the shore and drinks, setting the needs of nobody fails, and the night animal rests at noon and is about at midnight

### Requirement: Hunting prey
A hungry predator SHALL stalk the nearest prey of its food web that it sees or hears, give chase when close or when the prey bolts, bite it (harder for heavier hunters), and eat the kill; spent, it SHALL give up and leave prey alone for a while; a runner SHALL slow when spent; a death SHALL tell its killer.

#### Scenario: Hunting prey
- **WHEN** a hungry wolf meets a slow deer, and a tired wolf a fast one
- **THEN** the wolf stalks and chases, the deer sees it coming, the wolf kills it and the death names the wolf and the blows, the wolf then eats and the carcass is eaten away, and the tired wolf gives up the chase and leaves that deer alone

### Requirement: Herds and stampedes
Herds and packs SHALL keep apart, head the same way and stay together; when one of them takes fright the others near it SHALL flee with it.

#### Scenario: Herds and stampedes
- **WHEN** six deer of a herd start in one spot, and then one is hurt
- **THEN** after a while no two are closer than a metre and all are within 40 m, and when one is hurt all six flee

### Requirement: Scent trails
Every creature SHALL leave its scent in the air; a hungry hunter that smells it SHALL follow the strongest trail upwind.

#### Scenario: Following a scent upwind
- **WHEN** a breeze carries a deer's scent to a hungry wolf out of sight
- **THEN** the wolf picks up the scent and comes more than 25 m closer

### Requirement: Fauna in the game
Species SHALL take activity, thirst, stamina and prey from data; scripts SHALL read thirst, targets and eaten carcasses, set needs, and learn who killed whom; the mage on foot or low over the ground SHALL leave scent; the HUD SHALL tell kills.

#### Scenario: Fauna in the game
- **WHEN** at dusk a hungry pack downwind of a herd is left alone, thirsty deer wait by a pool, a wolf waits downwind of the mage on foot, and bad needs and species are given
- **THEN** the pack follows the scent and hunts, the herd runs together, the pack kills a deer and eats, the HUD tells the kill, the deer drink, the wolf smells the mage, and the bad arguments are refused
