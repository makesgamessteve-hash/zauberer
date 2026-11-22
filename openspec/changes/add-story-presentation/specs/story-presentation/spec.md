## ADDED Requirements

### Requirement: Speech with a speaker and choices
What a person says SHALL be shown in a framed box low on the screen with the speaker's name plate, going on with confirm; choices SHALL be numbered rows with their bindings, chosen by number, by up/down and confirm, by the mouse or by a tap; alone, the heir's actions SHALL wait while the box is open, in company nothing waits; every line spoken SHALL be kept in the journal.

#### Scenario: Talking to Farrokh
- **WHEN** the heir talks to Farrokh, whose words run over two boxes and end in three choices, and the second is chosen once by its number and, in a second run, by a tap
- **THEN** the plate names Farrokh, confirm goes on to the second box, the choices show numbered with chips, both ways choose the second, no cast happens while the box is open alone, and the journal holds the words and the choice

### Requirement: Narration and chapter cards
What the world does in the story SHALL be shown as framed narration under the compass, one line at a time for its reading time; a turn of the story SHALL show a card across the middle with the act, its name, the day and the hour, held for three seconds and skippable with confirm; the objective card SHALL announce the new goal.

#### Scenario: The night of the fire
- **WHEN** Azhdar arrives at dusk, the heir hides in the cellar and the next morning begins Act I
- **THEN** each line of the night shows as narration in turn, the card shows "Act I", its name and the day at dawn, confirm skips it, and the objective card shows the new goal marked as new

### Requirement: Place cards
Entering a settlement, ruin or story site for the first time SHALL show its name and kind in a small card under the compass, once.

#### Scenario: A market town
- **WHEN** the heir flies into Kesh, leaves and comes back
- **THEN** "Kesh" and its kind show the first time only

### Requirement: Story in company
Speech SHALL show to the heir spoken to; narration and chapter cards SHALL show to all; a choice that binds everyone SHALL be made by the host while the others see who they are waiting for.

#### Scenario: The moment of choice together
- **WHEN** two heirs reach the moment of choice and the host chooses
- **THEN** the client sees "Waiting for" and the host's name, and both see the chosen ending's card
