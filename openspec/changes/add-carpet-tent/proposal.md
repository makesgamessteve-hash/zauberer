# Proposal: The carpet as a tent, and the pocket home

## Why

The tent is the carpet's third mode (concept 2.3): a small canvas tent with a pocket space
inside, the home hub that grows over the game. M6's exit demo has the mage do three
things:
- pitch the tent in a storm;
- step inside into warm rooms;
- store goods in the vault.

## What Changes

- **The tent** (`flight/tent.lua`, `models/tent.glb`):
  - On foot by the carpet, P pitches it 2 m ahead. The ground there must be flat (no more
    than 0.5 m rise within 1.5 m), dry and not burning; otherwise pitching is refused with
    the reason.
  - The carpet folds into the tent: there is nothing to step on.
  - E at its door goes in. P by it packs it up at once, and the carpet lies there again.
  - The tent is a thing in the world: rain wets it and flames burn it (the carpet's
    condition).
- **The pocket home** (`player/home.lua`, `data/home_rooms.lua`, `models/room.glb`):
  - Rooms in a walking order. Open from the start: the hearth room (the fire never dies:
    cook there), the bedchamber (sleep safely and mend), the study (the Codex) and the
    vault (the bag's shelves).
  - Further rooms open as more space is folded (M7): pantry, alchemy laboratory, workshop,
    garden courtyard, ritual chamber, menagerie.
  - A/D walk between the open rooms and E goes out at the door.
  - Inside it is 20 C, sheltered and still, with no bites; time passes as outside.
  - In the vault, I shelves everything carried (the pack and the folds) and O takes the
    road's needs off the shelves, both freely.
- **The world outside** goes on around the tent while the mage is inside: ecology, the
  creatures (which do not see the mage), small life, and rain and flames on the tent.
- **Drawn:** the tent outside; inside, a tiled room with the rug and the hearth fire,
  checked as a golden image.
- **Saved:** the tent and the home are saved with the player.
- **Models:** a shared glTF writer draws both sides of every face (the renderer culls back
  faces).

- **The M6 exit demo** (`tests/scenarios/carpet_and_home.lua`): past a wary herd in the drab cloak, through a
  blizzard in it, the tent pitched in the storm, warm rooms, goods shelved in the vault and taken
  out through the bag 5 km away.

## Impact

- New capability `carpet-tent`.
- `harvest-and-cooking`'s cooking accepts the hearth; `endless-bag` gains the vault's
  shelves.
- Rooms open in M7 (alchemy, artificing, ritual magic); building kits in M8 will furnish
  them.
