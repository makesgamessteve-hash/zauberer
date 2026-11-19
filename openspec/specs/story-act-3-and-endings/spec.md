# story-act-3-and-endings Specification

## Purpose
Act III and the endings: divination of the citadel, readiness for the ash storm, the duel with Azhdar fought with the heirs' own magic, and four endings chosen after his fall.

## Requirements

### Requirement: The duel
A cast SHALL harm the foe by its power, 1.6 times for each colour it is weak to and 0.4 for each it wards, 1.5 times in a calmed storm; the foe SHALL strike at its pace; at no health it is defeated; the heir can yield.

#### Scenario: The duel
- **WHEN** Azhdar is struck with water, fire and in calm, and waits
- **THEN** water does most, fire least, calm more, his blows come at his pace, he falls, and a yielded duel is lost

### Requirement: Ready for the storm
The heirs SHALL be ready only with ritual mastery of 3, an ash ward and a summoned ally, and told what is missing.

#### Scenario: Ready for the storm
- **WHEN** readiness is asked lacking all and having all
- **THEN** what is missing is told, then they are ready

### Requirement: The endings
Unravel, Bind and Succession SHALL always be open after the duel; Reweave SHALL need ritual mastery of 3 and a void weave.

#### Scenario: The endings
- **WHEN** the endings are asked with and without deep mastery
- **THEN** three are open without and four with

### Requirement: Every ending can be reached
Each of the four endings SHALL be reached in the game from the moment of choice, completing the story.

#### Scenario: Every ending can be reached
- **WHEN** the story bot chooses each ending
- **THEN** each is taken and the story is complete
