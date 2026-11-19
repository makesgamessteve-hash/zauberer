## ADDED Requirements

### Requirement: Brewing by recipe
A recipe SHALL name its ingredients and its steps; brewing SHALL tell what is lacking, take the ingredients and mana, run the steps for their hours, bottle the products into the pack and make known what was in the vessel; alchemy's mastery SHALL give more from the same goods.

#### Scenario: Brewing by recipe
- **WHEN** the healing draught is brewed without ingredients, with them and life mana, without life mana, and by a more masterful alchemist
- **THEN** the lacking herbs and water are told, the draughts come into the pack with the hours of the recipe and the mana spent, without life mana no draught comes, and the master gets more

### Requirement: Spirit and quintessence
Spirit of wine SHALL be brewed from dates, and quintessence of light from spirit with light mana.

#### Scenario: Spirit and quintessence
- **WHEN** spirit is brewed from dates and then quintessence of light from it
- **THEN** flasks of spirit and the quintessence come, the light mana spent

### Requirement: Alchemy in the game
In the tent's laboratory the HUD SHALL show the chosen recipe with what is had of each ingredient; "." SHALL choose the next and K brew it while the hours pass; a drunk healing draught SHALL mend.

#### Scenario: Alchemy in the game
- **WHEN** the mage carries herbs and water into the laboratory, brews the healing draught, brews again with nothing left, and drinks a draught when hurt
- **THEN** the draughts are brewed and said, the hours have passed, what was in the vessel is known, the second brewing is refused for want of goods, and the mage is mended
