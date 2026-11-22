## ADDED Requirements

### Requirement: Zones of the HUD
The HUD SHALL show each kind of information in its own zone: the compass band at the top middle, the objective card at the top left, toasts at the upper right, the carpet's gauge at the right edge in flight, the two hands at the bottom corners with the mana ring on the left wrist and health with the needs above the left hand, and the prompt chip above the carpet's front edge, leaving the bottom middle to the carpet the rider sees.

#### Scenario: Flight with a goal
- **WHEN** the heir flies during the morning chores with two chores done, thirsty, holding Fireball and Kindle with five colours of mana, low enough to step off
- **THEN** the compass shows the heading and the goal's name and distance, the objective card lists the chores with two ticked, the gauge shows the height and speed, the hands show both weaves with their bindings, the ring has five segments with their glyphs and the total, the prompt chip offers stepping off with the device's binding, and the vitals show health and thirst

### Requirement: No free-floating text
No HUD text SHALL be drawn outside a frame, chip, plate or card, and nothing SHALL be drawn over the middle of the screen.

#### Scenario: Every text has a home
- **WHEN** the draw calls of the HUD in flight, on foot and in the tent are recorded
- **THEN** every text call lies inside a frame, chip, plate or card call of the same frame, and none lies in the middle third of the screen

### Requirement: Needs, toasts and prompts come when they matter
A need SHALL show only while it is worse than well; a pickup, discovery, save, sound caption, hint or quick message SHALL show as a toast that fades after its time, at most three at once and equal ones merged; the prompt chip SHALL show only while its action is possible, with the binding of the device last used.

#### Scenario: Events come and go
- **WHEN** the heir drinks until no longer thirsty, takes three pomegranates and four other things in a row, a wolf howls to the left, and comes low enough to step off
- **THEN** thirst leaves the vitals, the pomegranates merge into one toast, never more than three toasts show and each fades, the howl's caption shows as a toast, and the prompt chip shows the key bound to stepping off

### Requirement: The calm HUD
After 8 seconds with no event (news of the world going on around does not count) and no change of input, outside danger, the HUD SHALL draw back to the compass, the hands and health, dimmed; any event, input change or danger SHALL bring it back at once.

#### Scenario: Calm flight
- **WHEN** the heir flies straight for 10 seconds, then a toast arrives; and later flies on with a wolf 30 m away
- **THEN** after 8 seconds only the compass, hands and health show, dimmed; the toast brings the full HUD back; and with the wolf near the HUD never becomes calm

### Requirement: The HUD's state
The HUD SHALL gather what it shows into one state each frame, read by its zones and by tests; the game's lines of text SHALL no longer be drawn as the HUD (they remain its text for the headset's panel and the development panel), and lasting lines (rituals, a mod's lines, what is being looked at) SHALL be drawn in a framed status slot.

#### Scenario: State, not lines
- **WHEN** the HUD state is read in flight and on foot with a mod that adds a line
- **THEN** it holds health, needs, hands, mana, goal, places, prompt and toasts as values, the gauge only in flight, and the mod's line inside the status slot's frame

### Requirement: Co-op on the HUD
In company the compass SHALL show the other heirs as named ally marks, and quick messages SHALL arrive as toasts with the sender's name.

#### Scenario: Two heirs
- **WHEN** a second heir flies 300 m to the east and sends "Follow me"
- **THEN** the compass shows their mark to the east with their name, and a toast shows their name and the message
