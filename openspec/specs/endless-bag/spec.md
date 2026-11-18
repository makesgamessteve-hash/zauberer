# endless-bag Specification

## Purpose
Farrokh's endless bag: goods in folds without weight, slowed time, no living creatures, and one space with the tent's vault, reached through the bag for void mana.

## Requirements

### Requirement: Folds, not weight
The bag SHALL hold goods in folds of up to 50 of one item, 12 folds at the start, without weight; goods that find no fold SHALL be refused and told; living creatures SHALL be refused.

#### Scenario: Folds, not weight
- **WHEN** wood, eight kinds of goods and more are put in, the folds run out, a living and a dead creature are offered, and wood is taken out
- **THEN** 150 wood takes three folds, the eleven folds then used leave room for one more stack of fifty, what does not fit is refused and told, the living and the dead are refused, and the wood comes out

### Requirement: The vault through the bag
The bag and the tent's vault SHALL be one space: inside the vault goods SHALL move freely; elsewhere a stack SHALL be reached through the bag for 1 void mana, the bag then busy 3 seconds; without the mana nothing SHALL be reached.

#### Scenario: The vault through the bag
- **WHEN** meat is shelved in the vault and reached without a mage, with too little void mana, with enough, at once again, and after the bag rests, and goods are moved back from the vault
- **THEN** nothing comes without mana, the reach costs one void mana and brings the meat with its load, the bag is busy at once after, the next reach works, and shelved goods move back as far as the folds allow

### Requirement: Stowing and slowed time
The pack's goods SHALL be stowed in the folds; the road's needs SHALL come out as far as the pack carries, food and drink first; food SHALL spoil in the bag and the vault at a tenth of the rate; the bag SHALL be kept with the player.

#### Scenario: Stowing and slowed time
- **WHEN** a pack is stowed, the needs taken out of a bag with much wood, meat is kept a warm day in the bag and in the pack, and the bag is saved and read back, also from broken data
- **THEN** the pack is emptied into the folds, food and drink come out first and only what can be carried, the load comes along, the bag's meat is far fresher, and what was kept comes back

### Requirement: The bag in the game
I SHALL stow the pack's goods and O take the road's needs out, reaching the vault for food or drink the folds lack; the HUD SHALL show the folds and what happened.

#### Scenario: The bag in the game
- **WHEN** the pack is stowed, heavy wood added, the needs taken out, food and water shelved in the vault and taken out again with void mana, and the bag is saved
- **THEN** the pack empties into three folds and the HUD says so, food and drink come out and only what fits, the vault's meat and then its water come out for a void mana each, and the bag comes back from the save
