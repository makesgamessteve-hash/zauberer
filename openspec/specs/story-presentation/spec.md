# story-presentation Specification

## Purpose
TBD - created by archiving change add-story-presentation. Update Purpose after archive.

## Requirements

### Requirement: Speech with a speaker
What a person says SHALL be shown in a framed box low on the screen with the speaker's name plate, not as a toast; lines said one after another SHALL wait their turn, each shown for its reading time, and confirm SHALL go on to the next; the world and the heir SHALL go on meanwhile; every line said SHALL be kept in the journal.

#### Scenario: Talking to Farrokh
- **WHEN** Farrokh says two lines one after another and confirm is pressed once
- **THEN** the plate names Farrokh with his first line, no toast shows it, confirm goes on to the second line, the second goes after its reading time, and the journal holds both

### Requirement: Narration and chapter cards
What the world does in the story SHALL be shown as framed narration under the compass, one line at a time for its reading time; a turn of the story SHALL show a card across the middle with the act, its name and the day, held for three seconds and skippable with confirm; the objective card SHALL announce the new goal.

#### Scenario: The night of the fire
- **WHEN** the story's lines of the night are told and the story turns to the next part
- **THEN** each line shows as narration in turn, the card shows the act, its name and the day, confirm skips it, and the objective card shows the new goal marked as new

### Requirement: Place cards
Coming to a settlement, ruin or story site for the first time SHALL show its name and kind in a small card under the compass, once.

#### Scenario: A market town
- **WHEN** the heir comes to a village, leaves and comes back
- **THEN** its name and kind show the first time only
