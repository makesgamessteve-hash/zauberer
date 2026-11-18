# Proposal: Spellbook

## Why

Weaves can be cast, but not yet by a player. Concept 3.3 describes:
- free weaving with the full grammar, flexible and slow;
- prepared weaves inscribed in a spellbook, cast from the left and right hand slots or
  a quick wheel with just a gesture and a charge — the fast mode for flying combat.

New combinations are discovered by casting them, and familiarity grows with use. This
change puts magic into the flight game.

## What Changes

- **The spellbook** (`magic/spellbook.lua`):
  - the journal of discovered weaves in the order found, with how they were learned,
    their casts and familiarity tier;
  - up to 32 prepared weaves, the left and right hand slots and a quick wheel of eight;
  - news ("You discovered Unmake Fire", "Fireball is now Practised");
  - saved and restored with the game.
- **Magic in flight** (`magic/play.lua`):
  - a new game starts with the first weaves of Farrokh's books: Kindle, Light,
    Fireball and Gust, with Fireball in the left hand and Kindle in the right;
  - holding a hand button charges the hand's weave at the channeling rate, and letting
    go casts it along the carpet's heading;
  - a gesture drawn while charging sets control from its accuracy (halved for the
    wrong form), otherwise gesture assist gives fair control;
  - free weaving: words pick the verb, a first colour and an optional second one, and
    a drawn gesture sets the form;
  - the next left-hand cast within eight seconds is that free weave, and a free weave
    cast for the first time is journalled, prepared and put in the right hand;
  - the keys 1 to 8 put a quick-wheel weave in the right hand;
  - the HUD shows the mana by colour, the hands, the free weave, the charge, strain and
    news;
  - lights and bolts in flight are drawn, and so is the trail of a gesture being drawn;
  - the mage and the spellbook are saved with the game and are part of the state hash.
- **Bindings** (`data/input.lua`):
  - left and right hand: mouse buttons, shoulder or B, touch buttons;
  - draw: G or a stick click;
  - words: Q, Z and X, or the d-pad;
  - quick wheel: keys 1 to 8.
- The replay fixture's state hashes are recorded again: the state hash covers every
  input action and the magic.

## Capabilities

### New Capabilities

- `spellbook`: the journal, prepared weaves, hand slots and the wheel, and casting in
  play.

## Impact

- New: `magic/spellbook.lua`, `magic/play.lua`.
- Changed: `main.lua`, `data/input.lua`, `tests/replays/demo.zbrec(.hashes)`.
- Gathering mana (motes and channelling) follows in `add-mana-motes`. Until then the
  reservoir only spends.
