# Proposal: The endless bag

## Why

The child's limits (12 kg in the pack) make the endless bag the mage's real
store (concept 2.4). M6's exit demo has the mage store goods in the tent's vault and take
them out through the bag elsewhere.

## What Changes

- **The bag** (`player/bag.lua`):
  - Goods take folds, not weight: a fold holds a stack of 50 of one item, and there are 12
    folds at the start (upgrades add more later).
  - Time in the bag passes at a tenth, so food keeps longer but not for ever.
  - Living creatures are refused, with a word on the HUD.
- **The vault:** the bag and the tent's vault are one space.
  - Inside the vault, goods move freely between the folds and the shelves.
  - From anywhere, a stack is reached through the bag for 1 void mana. The bag is busy for
    3 seconds after each reach.
- **In the game:**
  - I stows the pack's goods in the folds.
  - O takes the road's needs back out as far as the pack can carry: food (the best kept
    first), drink, firewood, herbs, the rest. Food or drink the folds lack is reached from
    the vault.
  - The HUD shows the folds; the bag is saved with the player.

## Impact

- New capability `endless-bag`.
- The tent (`add-carpet-tent`) opens the vault's shelves; upgrades (`add-carpet-condition`,
  M7 artificing) add folds.
