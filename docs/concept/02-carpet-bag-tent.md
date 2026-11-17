# 2. The Carpet, the Bag and the Tent

The carpet, the *Wandering Weave*, is a Great Weave with a dormant will of its own. It bonds
to the child in Act I. It is cloth, so it can be singed, torn, soaked and repaired, and it
grows with the player through upgrades woven into it (see Artificing in [Magic](03-magic.md)).

## 2.1 Mode 1: Flight

### Flight model

The flight model is arcade-friendly physics, not a flight simulator.

- **Lift** comes from the carpet's own mana reservoir, which is mostly Storm and Void. The
  reservoir drains slowly in flight and refills from ambient mana, faster over ley lines.
  An empty reservoir means a gentle forced descent, never a fatal drop.
- **Thrust and drag.** The carpet has momentum and banks into turns. Air density decreases
  with altitude, so the carpet gets sluggish near its ceiling.
- **Wind interaction.** The local wind field from the weather simulation pushes the
  carpet. The player can ride thermals over sun-heated deserts and rocks, gets downdrafts
  on the lee side of mountains, and gets turbulence in storms. Expert players can use wind
  to save mana.
- **Load.** Passengers, carried creatures and heavy cargo outside the bag reduce climb rate
  and agility.
- **Altitude ceiling** starts at about 150 m above ground. Upgrades raise it to several
  kilometres. At high altitude the air is cold: the
  temperature lapse rate is about 6.5 °C per km, so the player needs warmth from the cloak
  or a spell.
- **Ground and water.** The carpet can hover low over water, skim treetops, and land
  anywhere flat enough.

### Flight abilities (unlocked over time)

| Ability | Unlock | Effect |
| --- | --- | --- |
| Hover | Start | Hold position in the air, which is needed for precise casting |
| Boost | Early | Burst of speed, costs carpet mana |
| Silent glide | Mid | No sound and a reduced visual signature; animals and guards notice less |
| Wind sail | Mid | Unfurl a corner to catch wind for efficient long-distance travel |
| Weatherproof weave | Mid | Ignore rain penalties; storms are survivable |
| Homing | Late | Autopilot to marked places (tent, domains, discovered sites) |
| High weave | Late | Ceiling up to cloud level and above for long-range travel and scouting |

### Carpet condition

The carpet has **integrity** (tears and burns) and **wetness**. A soaked carpet is heavier
and slower, and fire is the worst threat because carpets burn. Damage is repaired with
thread and Life or Void mana in the workshop, or patched in the field with lower quality.
The carpet never permanently dies. At zero integrity it falls, and the player must patch it
on the ground.

## 2.2 Mode 2: Cloak

On command, the carpet wraps around the player's shoulders as a hooded cloak.

- **Warmth.** The cloak has a high insulation value in the body-temperature simulation.
  It protects against cold nights, high altitude, snow and wind chill. It also
  keeps off rain.
- **Color shifting.** The cloak changes color and saturation on a single slider:
  - **Drab or camouflage:** the cloak samples the dominant colors of the surrounding
    terrain and vegetation from the biome palette at the player's position. Creatures and
    NPCs detect the player through a visibility model (light level, contrast against the
    background, movement, distance), and camouflage lowers contrast. This is the main
    stealth tool.
  - **Vivid or regal:** bright, rich colors and patterns. NPCs read it as "a mage". Some
    respect or fear this, some get hostile, and it can intimidate predators.
  - The pattern library grows in two ways. The player discovers dyes through alchemy and
    natural pigments, which are recorded in the Codex. The player also learns the
    carpet and textile motifs of the cultures they visit; as a weaver's child, they can
    weave any motif they have studied into the cloak.
- **Glide.** If the player falls from height while cloaked, the cloak spreads into a
  glider. This prevents fall damage and gives short-range gliding.
- **Ward lining** (upgrade). Artificing can weave passive wards into the cloak, such as
  fire resistance or a cushion against impacts.
- **Social.** In settlements, a cloaked player can walk around while the carpet is
  "hidden in plain sight". Flying into a village openly causes a reaction.

## 2.3 Mode 3: Tent (the pocket home)

On flat ground the carpet folds itself into a small, ordinary-looking canvas tent. The
player crouches to enter. Inside is a **pocket space** that is vastly larger than the tent.
It is built like the pavilion of a wealthy weaver's house in Farrokh's home culture:
carpeted rooms with tiled arches around a central courtyard with a walled garden and a
fountain.

### Rooms

The interior is a home hub that grows over the game. Rooms are unlocked by ritual magic and
artificing ("folding" more space) and by finding Farrokh's notes.

| Room | Purpose | Unlock |
| --- | --- | --- |
| **Hearth room** | Entry, fireplace, cooking, eating | Start |
| **Bedchamber** | Sleep (time skip, healing, dreams and visions), saving | Start |
| **Study / library** | Spellbooks, journal, Codex, research desk, map table | Start (Farrokh's books) |
| **Memorial alcove** | Family keepsakes, story mementos, trophies | Act I |
| **Pantry and cold store** | Food storage with a slowed-time shelf (reduced spoilage) | Early |
| **Alchemy laboratory** | Furnace, alembic, mortar, bain-marie, athanor, glassware | Early-mid |
| **Garden courtyard** | A walled four-part garden with an enchanted sun and a fountain; grow herbs, fungi and crops in chosen climates | Mid |
| **Workshop** | Workbench, loom, forge corner, enchanting table (artificing) | Mid |
| **Ritual chamber** | Inlaid circle for rituals, linked to the tent's ley anchor | Mid |
| **Menagerie** | Habitats for familiars and bound creatures | Mid-late |
| **Vault** | The physical side of the endless bag; walk-in storage with shelves | Mid |
| **Observatory** | Divination, star charts, scrying on the world map | Late |
| **Bath** | Warmth, healing, cleaning off poisons and grime | Optional |

The player can decorate rooms by placing furniture and trophies. Rooms are generated from
the same modular kit as the procedural buildings (see
[Settlements and architecture](06-settlements-architecture.md)), so mods can add new
rooms.

### Rules of the pocket space

- **Time.** Time inside passes normally. The outside world keeps simulating at reduced
  detail while the player is inside, using the dormant or near tier depending on distance
  (see [Simulation](05-simulation.md)).
- **The tent's location matters.** Mana regeneration inside depends on the outside spot.
  Pitching on a ley line is good. The tent's exterior is a physical object in the world:
  animals can sniff around it, rain falls on it, and a forest fire can threaten it.
  Pitching takes a moment, and packing up is instant. Wards from ritual magic protect the
  campsite.
- **No pitching** on steep slopes, in water, or inside buildings smaller than the tent.
- **Dreams.** Sleeping can trigger story dreams, where Farrokh's memories carry backstory
  and hints, and divination dreams, which hint at nearby resources or dangers.

## 2.4 The endless bag

Farrokh's bag looks like a small leather satchel. It holds a folded space.

- **Capacity by folds, not weight.** Items occupy slots ("folds") with stack limits, and
  carry weight doesn't apply to items in the bag. The number of folds grows with Void
  mastery and artificing upgrades.
- **Linked to the vault.** The bag and the tent's vault are the same space. Anything in the
  vault is reachable from the bag, but taking items out of the vault through the bag
  costs a little Void mana and some time. Organizing happens in the vault.
- **Slowed time.** The bag slows time to about one tenth, so food rots slower. Upgrades
  push this further, but there is never perfect stasis, so preservation through salting,
  drying, smoking and alchemy stays useful.
- **No living animals.** Creatures cannot enter the bag (seeds, cuttings and fungal spores
  can). Trying it gives a visible refusal effect. Living animals travel on the carpet or
  in the menagerie.
- **Loss protection.** The bag can't be stolen or lost. It is soul-bound like the carpet.

## 2.5 The carpet as a companion

- **Bond level** rises with time spent flying, repairing and resting together. Higher bond
  unlocks abilities faster and enables small autonomous behaviours. For example, the
  carpet nudges the player awake when danger approaches the tent, or comes when called
  (whistle) within a radius.
- **Personality.** The carpet is not a talking companion. It has expressive animations:
  ruffling, curling corners and shivering in the cold.
- **Antagonist pull.** Azhdar can sense the carpet when it's used near his agents or in
  high-Void areas. The silent-glide and ward upgrades counter this.

## 2.6 Optional extra modes (proposals, not committed)

| Mode | Idea | Discipline |
| --- | --- | --- |
| **Shield** | The carpet stiffens into a curved barrier in front of the player | Artificing (Earth + Void) |
| **Raft** | Floats as a raft on water; sail with the wind | Artificing (Water) |
| **Bridge / rope** | Unrolls into a rigid plank or a long rope for climbing | Artificing (Earth) |
| **Mount for two** | Carry an NPC passenger for escort quests and rescues | Upgrade |
