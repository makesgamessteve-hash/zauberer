# carpet-tent Specification

## Purpose
The carpet folded into a tent with the pocket home inside: pitching, the rooms and what they offer, the vault's shelves, the world going on outside, and the M6 exit demo.

## Requirements

### Requirement: Rooms of the pocket home
The pocket home SHALL have rooms from data in a walking order, the hearth room, bedchamber, study and vault open from the start and others opened later; each SHALL offer what it is for; inside it SHALL be 20 C, sheltered and still, the hearth burning.

#### Scenario: Rooms of the pocket home
- **WHEN** a new home is walked through both ways, its warmth felt in the bedchamber and by the hearth, the laboratory opened twice and a ballroom asked for, and the home is saved and read back, also from broken data
- **THEN** the four open rooms are walked through and the closed ones passed by, the bedchamber offers sleep and the hearth cooking, inside is warm, sheltered and still with a fire at the hearth, the laboratory opens once and is told, the ballroom does not exist, and the home comes back

### Requirement: The vault's shelves
In the vault, everything carried SHALL be shelved and the road's needs taken off the shelves into the pack as far as it carries, freely.

#### Scenario: The vault's shelves
- **WHEN** a pack of meat and hides and a bag of wood are shelved and the needs taken off again
- **THEN** all goes onto the shelves, the meat and water come off and the hides stay, and only what can be carried

### Requirement: The tent in the game
On foot by the carpet P SHALL pitch the tent on flat, dry, unburning ground 2 m ahead (else refuse with the reason), the carpet folded into it; E at its door SHALL go in and out; inside, A/D SHALL walk the rooms, the bedchamber give a warm, mending night, the vault keep the shelves and the hearth cook, while the world outside goes on; P by the tent SHALL pack it up; the tent and home SHALL be saved.

#### Scenario: The tent in the game
- **WHEN** the mage tries to pitch over a puddle, pitches on dry ground, tries to mount, goes in, walks to the bedchamber and sleeps tired while it freezes outside, shelves meat in the vault and takes it back, cooks at the hearth, goes out and packs up
- **THEN** the puddle is refused and told, the tent stands with the carpet in it and nothing to mount, the HUD shows the hearth room, the mage sleeps, is rested, warm and mended, the meat is shelved and taken back, the raw meat is cooked, the mage stands at the door, the carpet lies where the tent was, and the tent comes back from a save

### Requirement: Inside the tent
Inside, the game SHALL draw the room of the pocket home the mage is in, with its floor, walls, ceiling, the rug and the hearth's fire.

#### Scenario: Inside the tent
- **WHEN** the mage pitches the tent and goes in, without the HUD
- **THEN** the hearth room matches its golden image

### Requirement: The M6 exit demo
The game SHALL let the cloaked mage pass a wary herd unseen in drab (seen and fled from in vivid), keep warm through a blizzard in the cloak, pitch the tent in the storm and warm up in its rooms, shelve goods in the vault, and take them out through the bag far away.

#### Scenario: The M6 exit demo
- **WHEN** the mage in a drab cloak walks past five deer 50 m off, shifts to vivid closer, walks six hours through a blizzard eating and drinking, pitches the tent in it, goes in cold and stays four hours, shelves meat and water in the vault, packs up, flies 5 km on and takes out the road's needs
- **THEN** no deer sees or flees from the drab mage and most flee the vivid one, the mage never falls below 35.5 C nor is hurt by the cold, the tent stands in the storm and the mage warms inside while it freezes outside, the goods are shelved, and the meat and then the water come out through the bag for a void mana each
