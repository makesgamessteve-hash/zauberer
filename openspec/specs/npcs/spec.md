# npcs Specification

## Purpose
The people of settlements: households, daily life, how they see the heirs, talk by topics, services and requests.

## Requirements

### Requirement: People of a place
A laid-out place SHALL be peopled by households of named people (parents, children, elders) with trades, faiths and traits, the same each time.

#### Scenario: People of a place
- **WHEN** Anarvan is peopled twice
- **THEN** it has families of weavers and the same people both times

### Requirement: The day of a place
People SHALL sleep at night, eat, work, pray at their faith's hours, play on the square (children), keep festivals, shelter from rain, fight fires and go home in raids, and walk to where the hour takes them near an heir.

#### Scenario: The day of a place
- **WHEN** the hours of a parent and a child are asked and the game runs to mid-morning
- **THEN** each is where the hour says, and children play on the square

### Requirement: How people see the heirs
People SHALL see the heirs warmly or warily by standing, cloak, magic seen and their faith and people, and turn against those who break guest right.

#### Scenario: How people see the heirs
- **WHEN** a person is asked their mood with standing, broken guest right and seen magic
- **THEN** standing warms them, broken guest right chills them, and magic is feared where it is an abomination

### Requirement: Talking by topics
Talking SHALL begin with a greeting and go through topics (rumours, directions with distance and bearing, history, faith, songs, grudges, prices, work); met people are remembered and saved.

#### Scenario: Talking by topics
- **WHEN** the heir talks with a villager nine times
- **THEN** the topics come in turn with true directions and prices, and the meeting is remembered

### Requirement: Services
Innkeepers SHALL give lodging, priests and herbalists healing, herbalists and hedge mages teaching, merchants trade, herders and merchants guiding, for coin.

#### Scenario: Services
- **WHEN** each service is asked with and without coin
- **THEN** without coin the price is told, with it the service is done

### Requirement: Requests from the world
Requests SHALL come from the world's state: predators, wildfire, drought (call rain), bandits, a poisoned well (a clue), now and then a lost child.

#### Scenario: Requests from the world
- **WHEN** a place with every trouble is asked
- **THEN** each request is made
