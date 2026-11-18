## ADDED Requirements

### Requirement: Seen or not
Creatures SHALL see the player at their daylight sight range times the player's visibility (0 .. 2) and be frightened within their flight distance (default 25 m) by the player's presence (0 .. 3); the cloak SHALL set visibility from its contrast (drab, plain, vivid) and the mage's movement, the carpet in the air showing at 1.5 (halved by the silent glide), and vivid SHALL awe.

#### Scenario: Seen or not
- **WHEN** a wary deer that keeps 60 m is 40 m from a camouflaged mage and then a plain one, a plain deer is 40 m off, a deer close by meets a plain and an awing mage, and values out of range are set, and a cloak's colours, movement and flight are weighed
- **THEN** the camouflaged mage is not seen and the plain one is and the deer flees, the plain deer stays calm, awe frightens more, the limits hold, drab and still hides most, vivid shows and awes, and the carpet in the air shows unless silent

### Requirement: Warmth in the cloak
Worn, the cloak SHALL warm the body by up to 20 degrees as far as it wants (less when wet), halve the wind's chill and keep off rain.

#### Scenario: Warmth in the cloak
- **WHEN** two bodies spend eight hours walking in a blizzard, one in the cloak, and the cloak is weighed wet and in the heat
- **THEN** the bare body freezes and is hurt while the cloaked one stays warm and dry and is hurt less, wet it warms less, and in the heat it adds nothing

### Requirement: The cloak in the game
On foot by the carpet H SHALL wrap it on (no carpet left to step on) and spread it out beside the mage; in the air H SHALL wrap it and the mage glide down unhurt; "." SHALL shift its colour; the HUD SHALL show it, the creatures see the mage by it, and the save keep it.

#### Scenario: The cloak in the game
- **WHEN** the mage steps off, wraps the cloak, tries to mount, shifts it to drab and stands 45 m from a deer at noon, shifts it to vivid, spreads it out, flies 60 m up and wraps it, and it is saved
- **THEN** it is worn and the HUD says so, there is no carpet to mount, the drab mage is not seen, the vivid one is and the deer flees, the carpet lies beside the mage, the mage glides down unhurt, and the cloak comes back from the save
