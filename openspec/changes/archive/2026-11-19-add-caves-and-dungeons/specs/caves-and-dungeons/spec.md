## ADDED Requirements

### Requirement: Caves from a cave graph
A cave SHALL be chambers each lower than the last joined by passages with a loop and a branch, all reached from its mouth, carved as voxels; karst has formations, a spring, a lake and an outflow; every kind is connected.

#### Scenario: Caves from a cave graph
- **WHEN** caves of every kind are made
- **THEN** they are connected with their loops and branches and karst has its river

### Requirement: Walking in a cave
Cave floors SHALL be smooth: level with the ground at the mouth, easing down the entrance, never a step higher than the walker can take.

#### Scenario: Walking in a cave
- **WHEN** the floor is walked from the mouth to the first chamber
- **THEN** it starts at the ground and never steps too high

### Requirement: The cave's own simulation
Water SHALL fall and spread through open cells, kept but for the spring and the outflow; heat and foul air SHALL diffuse to neighbours and fade into the rock.

#### Scenario: The cave's own simulation
- **WHEN** water, a torch's heat and foul air are released
- **THEN** water is kept and falls, the spring adds, heat spreads and fades, foul air spreads

### Requirement: Dungeons from a graph grammar
A dungeon's mission graph SHALL always be solvable, every lock's key reachable before it and the lock barring the goal without it; loops do not go round locks; branches are optional.

#### Scenario: Dungeons from a graph grammar
- **WHEN** every kind is made with twelve seeds and its keys taken away
- **THEN** all are solvable and each lock bars the way without its key

### Requirement: Laid out and built
Rooms SHALL lie on a grid, one per cell, joined by straight or turning corridors, built of floors, walls with openings, doors at locks and barriers at puzzles.

#### Scenario: Laid out and built
- **WHEN** every kind is built
- **THEN** no two rooms share a cell and there is a door per lock and a barrier per puzzle

### Requirement: Puzzles of the simulation
An ice door SHALL melt with heat, thorns burn, a flood lift to the ledge at 2 m, a crystal wake with light, gas plates be passed with care, torches be passed in the dark.

#### Scenario: Puzzles of the simulation
- **WHEN** each puzzle meets and misses what it needs
- **THEN** it gives way only to what it needs

### Requirement: Caves in the land
Cave mouths SHALL be found by region, the same each time, and a cave made when an heir comes within 60 m of its mouth.

#### Scenario: Caves in the land
- **WHEN** the land about Anarvan is searched and an heir comes to a mouth
- **THEN** there is a mouth and its cave comes to be

### Requirement: Down into a cave
The heir SHALL walk from the mouth down under the ground onto the cave's floors while its river runs.

#### Scenario: Down into a cave
- **WHEN** the heir walks into a cave
- **THEN** they are under the ground on its floor and its water moves

### Requirement: Into a dungeon
Dungeons SHALL lie under tombs and other sites; entered by a stair; a locked door SHALL open with its key, a puzzle give way to what is brought, treasure be found at the goal.

#### Scenario: Into a dungeon
- **WHEN** a tomb is entered and played
- **THEN** the stair leads down, the key opens the door, the puzzle gives way and the treasure is found
