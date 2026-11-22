# game-audio Specification

## Purpose
TBD - created by archiving change add-game-audio-and-haptics. Update Purpose after archive.

## Requirements

### Requirement: Music and ambience by situation
The music SHALL follow the situation (the title or ending, a moment of the story, a hunter near or low health, night, else calm flight) and the ambience the place (underground, rain, aloft, a settlement, night), each crossfading over two seconds.

#### Scenario: Danger near
- **WHEN** a wolf comes near the heir in calm flight and is then gone, and the story turns to Azhdar's arrival
- **THEN** the danger music fades in over two seconds while the calm fades out and stops, calm returns after, and the story's music plays for the arrival

### Requirement: Feedback for actions
Casting, a weave going off, a hit taken, a pickup, a defeat and a turn of the story SHALL each play a sound; with a gamepad, unless vibration is off, hits, misfires and defeats SHALL rumble it in proportion to the harm.

#### Scenario: Bitten with a gamepad
- **WHEN** the heir takes a hit of 12, picks something up, is defeated, is hit again with vibration off, and the story turns
- **THEN** each is heard; the first hit rumbles at 12/30, the hit with vibration off does not, and bad rumble strengths are refused
