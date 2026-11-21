## ADDED Requirements

### Requirement: A defeated player is spared
When the player is defeated, the creatures hunting the player within a radius SHALL turn away and leave the player alone for a while; scripts SHALL ask for it with a radius up to 500 m and up to an hour, and get how many did.

#### Scenario: A defeated player is spared
- **WHEN** a wolf hunting a mage on foot has reached the mage and the mage is defeated; and when no one hunts, or there is no player
- **THEN** the wolf stops hunting; none is counted when no one hunts or there is no player
