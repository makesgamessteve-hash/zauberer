# ui-mood Specification

## Purpose
TBD - created by archiving change add-ui-mood-and-feedback. Update Purpose after archive.

## Requirements

### Requirement: Frames take the light of the hour
HUD frames SHALL be tinted by the light and the weather (warm when the sun is low, lapis at night, shaded under a storm) by at most 20 %, without changing the colours that carry meaning (mana, health, the goal); high contrast SHALL not be tinted.

#### Scenario: Dusk and night
- **WHEN** the tint is taken at noon, with the sun low, at midnight and under a storm
- **THEN** it is white at noon, warm with the sun low, lapis at midnight and shaded in the storm, each channel within 20 % of white, and the mana colours are the same in all four

### Requirement: Danger at the edges
While a hunter is near the screen's edges SHALL warm with a pomegranate band; under 30 % health it SHALL beat slowly (steadily with reduced flashing); a hit SHALL mark the edge it came from for a moment.

#### Scenario: A wolf closes in
- **WHEN** a wolf is near, a hit comes from the rider's left, and health falls under 30 %
- **THEN** the edges glow, the left edge is marked for a moment, and the glow beats

### Requirement: UI sounds
The UI SHALL have generated, credited sounds for a toast appearing, a page of the book turning and a new goal, played at the effects volume.

#### Scenario: A goal ticked
- **WHEN** a toast appears, the book's page is turned and a new goal is set
- **THEN** the cloth, the page and the goal's chime are heard
