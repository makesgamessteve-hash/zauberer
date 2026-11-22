## ADDED Requirements

### Requirement: Frames take the light of the hour
HUD frames SHALL be tinted by the time of day and weather (warm at dawn and dusk, lapis at night, shaded under a storm) by at most 20 %, without changing the colours that carry meaning (mana, health, the goal).

#### Scenario: Dusk and night
- **WHEN** the HUD is drawn at noon, at dusk, at midnight and under a storm
- **THEN** the frames are untinted at noon, warm at dusk, lapis at midnight and shaded in the storm, each within 20 % of the plain frame, and the mana colours are the same in all four

### Requirement: Danger at the edges
While danger is near the screen's edges SHALL warm with an ornamental vignette that grows as it closes in; under 30 % health it SHALL beat slowly; damage SHALL mark the edge it came from for a moment.

#### Scenario: A wolf closes in
- **WHEN** a wolf comes from 40 m to 5 m on the left and bites, and health falls under 30 %
- **THEN** the vignette appears and grows, the left edge is marked on the bite, and the vignette beats

### Requirement: Feedback and UI sounds
Frames SHALL unfold when they appear, toasts slide in, ticks pop, the mana ring pulse on a change of mana and a hand flash when its weave goes off; the UI SHALL have generated, credited sounds for a frame appearing, a page turning, a new goal and a journal entry, at the effects volume; reduced motion SHALL make every movement a plain appearance.

#### Scenario: A goal ticked
- **WHEN** a chore is done, with reduced motion off and then on
- **THEN** the tick pops and the chime plays; with reduced motion the tick appears without the pop and the chime still plays
