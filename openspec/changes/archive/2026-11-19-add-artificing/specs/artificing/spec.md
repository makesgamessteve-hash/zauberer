## ADDED Requirements

### Requirement: Enchanting
An enchantment SHALL name the thing it goes into and its mana, costing less the better the thing conducts its colours; enchanting SHALL tell what is lacking, take the thing and mana, and make an artifact, a carpet upgrade (refused when already woven) or more bag folds; artifacts SHALL be saved.

#### Scenario: Enchanting
- **WHEN** a lantern is enchanted without the lantern or mana and with them, the weatherproof weave woven twice, the folding knot tied, and the artifacts saved and read back
- **THEN** the lacking lantern and mana are told, the horn lantern's conduction lowers the cost, the artifact is made charged, the weave goes into a slot once and is refused again, the bag gains six folds, and the artifacts come back

### Requirement: Artifacts at work
An artifact SHALL draw the ambient mana of its colour around it into its charge and burn its charge while it works, and go dark when it is spent.

#### Scenario: Artifacts at work
- **WHEN** the lantern burns with no mana around it
- **THEN** it burns its charge down and goes dark, and the HUD says it is dark

### Requirement: The workshop in the game
In the tent's workshop the HUD SHALL show the chosen work with what is had; "." SHALL choose the next among the crafts and enchantments and K do it; outside, the lantern's light SHALL be drawn and burn on the land's mana.

#### Scenario: The workshop in the game
- **WHEN** the mage makes a cord and a horn lantern, enchants it with light, mends the carpet, and goes out for a day and a night with no light mana of their own
- **THEN** each work is made and said, the carpet is whole, and the lantern stays alight all day and night, told on the HUD
