# narrative-director Specification

## Purpose
The narrative director places the story in each generated world: it anchors the Circle's remnants in the world's own history, spreads rumours that lead to them, paces the acts and keeps the story's state with the save.

## Requirements

### Requirement: Beats anchored in the world
Each Circle member's beat SHALL be anchored to a site of its kind (a settlement of the right land, a tower, a grave, an elder ruin), the one nearest 60 km from home with ties by the seed, the same every time, no site twice; the story SHALL open with the trail of ash.

#### Scenario: Beats anchored in the world
- **WHEN** the director anchors twice
- **THEN** every member has a site, the same both times, none shared, and the trail of ash is the first open beat

### Requirement: Rumours and reunions
Rumours SHALL point to the next remnant by direction and distance; with none left there is none; an heir joining late SHALL be met at the nearest settlement still standing.

#### Scenario: Rumours and reunions
- **WHEN** rumours are asked before and after all remnants are found, and a reunion for a late heir
- **THEN** the rumour points the way, then there is none, and the reunion is at a standing place

### Requirement: The saga in the game
After the prologue the saga SHALL begin with Azhdar's first burning (the trail of ash), its mages and the nexus the citadel drifts round; following the trail completes its beat; divination without three Weaves tells why not; the saga SHALL be kept with the save.

#### Scenario: The saga in the game
- **WHEN** the saga begins, the heir stands at the trail, tries to divine and the game is saved
- **THEN** the trail beat is done, divination is refused with why, and the saga comes back

### Requirement: The M10 exit demo
A story bot SHALL play the whole story headlessly on the fixed seed - the prologue, the trail, the Circle, a rival, divination, readiness, the duel with real casts - and from the moment of choice each of the four endings.

#### Scenario: The M10 exit demo
- **WHEN** the story bot plays
- **THEN** every beat completes and each ending is reached with its consequences

### Requirement: The journal of goals
A journal page SHALL show the current goal with its place, distance and direction, the requests people made with who asked and where, what is finished, and the last 100 lines said with their game time, newest first; a line repeated is noted once and a request once per place; the journal SHALL be kept with the save.

#### Scenario: Goals and what was said
- **WHEN** a line is said twice, a request is heard twice in a village, a story beat is finished, the page is opened, 150 lines are noted and the journal is saved and restored
- **THEN** the line is noted once with its time, the request once with its giver and place, the beat under finished, the page shows the goal, request, finished and lines, 100 lines are kept, and all come back from the save

### Requirement: Hurrying a story moment
During a timed story moment (Azhdar's arrival, the hiding) holding use for a second SHALL hurry it to its end, keeping every line it says.

#### Scenario: Hurrying the hiding
- **WHEN** use is held during Azhdar's arrival, and the hiding is hurried
- **THEN** the arrival ends with his demand said, the hiding ends with every fragment said, and nothing is hurried in the village
