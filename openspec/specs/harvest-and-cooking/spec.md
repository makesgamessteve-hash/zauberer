# harvest-and-cooking Specification

## Purpose
Living off the land: food and drink as data, the pack the mage carries, gathering from carcasses, plants, water and mushrooms, campfires, cooking, boiling and brewing, and eating and drinking.

## Requirements

### Requirement: Food and drink
Items SHALL carry their energy, water, raw risk, what cooking, boiling and brewing make of them, poison, cures, dirt, salt, how they spoil and their firewood value; references to other items SHALL exist and values SHALL be in range, or the registry is refused.

#### Scenario: Food and drink are data
- **WHEN** the base items are read and broken entries are checked
- **THEN** meat cooks into better meat, swamp water boils clean and is dirtier than river water, herbs brew a curing tea, sea water is salty, the fly agaric poisons, wood burns, and each broken entry is refused with its reason

### Requirement: A campfire
Dry wood laid on a cell SHALL be fuel of the wood kind, dry enough to light, also on ground burnt before, never under water; lit, it SHALL burn.

#### Scenario: A campfire
- **WHEN** wood is laid on dry ground, lit, put out, laid and lit again, nothing is laid, and wood is laid on a river
- **THEN** the cell holds dry wood, burns, takes wood again and burns again, nothing laid takes nothing, and the river takes none

### Requirement: The pack
The mage SHALL carry up to 12 kg outside the bag, a stack per item with its microbe load; what does not fit SHALL be left behind and told; food SHALL spoil in it with game time, warmth and wet, slower cooked; the pack SHALL be saved with the player.

#### Scenario: The pack
- **WHEN** more wood is packed than fits, raw and cooked meat are kept a warm day, and the pack is saved and read back
- **THEN** only what fits is packed and the rest is told, the raw meat is spoilt and the cooked less so, and the saved pack weighs the same

### Requirement: Gathering
On foot the mage SHALL gather within 3 m: butcher a carcass into its products (less what was eaten), fill four cups of water straight ahead (facing it, before the plants about), take a part of a plant (no trees felled by hand, more fruit where bees are), fill four cups at the water's edge of a kind by the water, or pick the mushrooms of the patch.

#### Scenario: Gathering
- **WHEN** the mage on foot gathers by a plant with something to give, by a dead hare, and at a pool with nothing else in reach
- **THEN** the plant gives and the HUD says so, the hare gives three meat and a hide and is used up, and four cups of pool water are filled

#### Scenario: Facing the water
- **WHEN** the mage at the edge of a pool, with plants to give within reach, gathers facing the water
- **THEN** four cups of pool water are filled, not a part of the plants about

### Requirement: Cooking, eating and drinking
A campfire SHALL be laid from the pack's firewood; over a fire within 3 m raw food SHALL cook, water boil and herbs steep in boiled water into tea; eating and drinking SHALL take the best of the pack: cooked before raw, remedies when sick, clean water before dirty, the sea last.

#### Scenario: Harvest and cooking in the game
- **WHEN** the mage steps off by a plant and gathers, butchers a hare, fills cups at a pool, tries to cook without a fire, lays a campfire and lights it with Kindle, cooks, eats when hungry, drinks when sick, overfills the pack, leaves meat out on a warm day, and the pack is saved
- **THEN** the plant gives and the HUD says so, the hare gives three meat and a hide and is used up, four cups of pool water are filled, cooking without a fire is refused, the campfire burns, the meat is cooked, the water boiled and tea brewed, cooked meat is eaten, the tea is drunk first and helps, only what fits is packed and the rest told, raw meat spoils faster than cooked, and the saved pack comes back
