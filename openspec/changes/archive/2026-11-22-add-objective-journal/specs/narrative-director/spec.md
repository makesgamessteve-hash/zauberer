## ADDED Requirements

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
