## ADDED Requirements

### Requirement: The player's health
The player SHALL have health up to 100, hurt through heat, impact, cold, poison, smoke and magic: flames and heat under a player on foot or within 3 m of the ground, thick smoke, bites while on foot, hard landings and weaves going off in the hand. A ward of the harm's colour SHALL let only 30 % through; healing SHALL mend up to full health, and health SHALL return slowly after ten seconds unharmed.

#### Scenario: Harm to the player
- **WHEN** the carpet hovers low over a fire, with and without a ward of Fire, the player is healed, and a weave goes off in the hand
- **THEN** the fire burns, the ward turns most of it aside, healing mends, the backfire costs half its power in health, and the HUD shows health and what happened

### Requirement: Magic against creatures
A bolt passing within 1.5 m of a living creature SHALL strike it (fire burns, water and stone strike) and go off there; withering SHALL hurt and healing mend the creatures within its reach; a spark SHALL jolt what it touches.

#### Scenario: Magic against creatures
- **WHEN** a Fireball is cast at a deer in its path, and a wolf is withered and then healed
- **THEN** the deer is burnt, the wolf loses health and then gains some back

### Requirement: Defeat
At no health the player SHALL be defeated: controls rest for three seconds, then the player gets up at half health where the carpet first set out; health and the number of defeats SHALL be saved with the player; unknown harms SHALL be refused.

#### Scenario: Defeat
- **WHEN** the player takes more harm than health is left, waits, and the health is saved and restored
- **THEN** the player is defeated, told so, gets up at half health where the carpet set out, and the saved health and defeats come back
