# 11. Playing Together: Co-op for up to Four Heirs

Zauberer can be played alone or in online co-op with **up to four players** sharing one
world. All players are **Farrokh's children, the heirs of the Wandering Weave**. Co-op is
part of 1.0. Single-player is the same game with one heir.

## 11.1 The heirs

- At world creation, the host sets the number of heirs (1–4). Each player creates their
  heir: girl or boy, name, appearance and age (siblings between about 9 and 14; twins
  are possible).
- **Prologue with several heirs:** Farrokh hands the rolled-up carpet to *all* his
  children and sends them into the cellar together. The hiding sequence lets each heir
  watch through a different crack in the floor, so players see different fragments of
  the attack and can piece them together afterwards.
- **Heirs who join later** were away when Anarvan burned: apprenticed in a city, visiting
  relatives, or travelling with a caravan. The narrative director inserts a **reunion
  beat** at a suitable settlement. The newcomer has their own short "away" prologue (the
  news arriving, the journey back) and then joins the others. This keeps drop-in co-op
  consistent with the story at any point in the campaign.
- **Absent heirs:** when a player is offline, their heir **stays in the pocket home**
  (resting, studying, tending the garden) and is out of the world. Nothing is lost.
  Nothing advances for them except time-based growth such as the garden and aging.

## 11.2 One carpet, several heirs

There is only one Great Weave, so the carpet shares itself:

- **Riding together:** the full Wandering Weave carries all heirs. One heir pilots and the
  others are free to cast in every direction, like a flying war-chariot. This is the
  strongest flight mode: full lift, speed and mana reservoir.
- **Splitting the Weave:** the carpet can **unravel into up to four smaller carpets**, one
  per heir, each bonded to its heir. Split carpets are individually weaker. Lift, speed,
  ceiling and the mana reservoir are divided, scaled by each heir's bond. Splitting and
  rejoining takes a moment, and rejoining requires the heirs to be close together.
- **Cloaks:** each heir's piece becomes their own cloak, with its own colors and learned
  patterns.
- **The tent is always whole.** Pitching the tent requires the Weave to rejoin, so the
  heirs return to one shared pocket home at night. Heirs far apart can't pitch until they
  regroup, or until the late-game **Homing** upgrade lets a split carpet fly back to the
  tent on its own.
- **Damage and upgrades:** upgrades woven into the Weave apply to every piece. Damage to a
  piece carries over to the whole when it rejoins, so tears must be mended.

## 11.3 What is shared and what is personal

| Shared by all heirs | Personal to each heir |
| --- | --- |
| The world, its history and its simulation | Body: health, hunger, warmth, fatigue, conditions |
| Story progress, journal of story events | Disciplines, mastery, weave familiarity, attunement, mana reservoir |
| The pocket home and its rooms, the vault | Spellbook annotations, prepared weaves, hand slots |
| The endless bag's fold (each heir carries a satchel that opens into the same fold) | Equipment, artifacts worn, the cloak's look |
| Domains, sanctums, servants | Familiar (one each) |
| Codex discoveries (shared library) | Reputation with NPCs who met them personally (rest is family reputation) |

Knowledge is shared through the **library** in the pocket home. A Codex entry or a
spellbook page discovered by one heir is readable by all. Each heir still has to
*practise* a weave before it becomes familiar to them, so specialisation pays off: one
heir as an alchemist, another as a summoner.

## 11.4 Time, sleep and pausing

- The world runs on the **host's clock**. The game pauses only in single-player, or when
  the host pauses a co-op session for everyone.
- **Sleeping and time skips** (sleep, long rituals, waiting) need all present heirs to be
  in the pocket home or to agree in a quick vote. Heirs outside can ask the others to
  wait.
- Long rituals can be **channelled together**. Each extra heir adds mana and shortens the
  duration, which makes rituals a natural co-op activity.

## 11.5 Difficulty and scaling

- Creature group sizes, predator boldness and the strength of hostile mages scale with
  the number of present heirs. The world's resources and mana don't scale. Four heirs
  drain a region's mana faster, which is part of the ecological theme.
- Friendly fire is **on by default**, because spells are physical and a fireball burns
  whoever stands in it. A world option reduces damage to other heirs.
- Revival: a downed heir can be revived by another heir with Life magic or medicine
  before the carpet carries them away.

## 11.6 Sessions and connectivity

- **Hosting:** one player hosts the world and the save lives on the host's machine. Other
  players join with an **invite code** (through a relay) or by direct address on a LAN.
- **Cross-play:** Windows, Linux, macOS, browsers (desktop and mobile) and WebXR players
  can play together in one session.
- **Joining:** on first join, the client downloads the world bundle (10–30 MB, cached for
  later sessions) and generates terrain, vegetation and buildings **locally** from the
  bundle and seed. Only changes and moving things travel over the network.
- **Drop-in and drop-out** at any time. If the host leaves, the session ends and is
  saved. Host migration is not planned for 1.0.
- **Mods:** all players need the same package set. On joining, mismatches are listed, and
  packages are never transferred automatically, because scripts are code.
- **Hosting recommendations:** a minimum-spec desktop hosts four heirs who stay
  together. Heirs spread across the planet each need their own active simulation area,
  so the host automatically reduces each area's radius. A 6-core host is recommended for
  four heirs who split up. Mobile browsers can join, but should not host more than one
  guest.
- **Communication:** pings on the world and map, emotes and gestures, and a short text
  chat. Voice chat is left to external tools in 1.0.
- **Not in 1.0:** split-screen or local same-screen co-op, public matchmaking, more than
  four players, and host migration.

## 11.7 How co-op shapes the engine

The details are in the [implementation plan](../implementation-plan.md); this is the
summary.

- **Host-authoritative simulation:** the host runs the authoritative simulation. Clients
  predict their own heir and carpet locally for responsive flight, and are corrected by
  the host.
- **Deterministic generation saves bandwidth:** clients generate the world locally from
  the bundle and seed, so the network carries only deltas (terrain edits, felled trees,
  burnt cells, opened doors) and entity states in each client's area of interest.
- **Several active simulation areas:** the tier system (see
  [Simulation](05-simulation.md)) supports one active area per separated heir, sharing a
  CPU budget.
- **Built in from the start:** player indices are on input snapshots, entities have
  owners, and nothing in the engine assumes a single "the player". Every gameplay
  feature's specification states its co-op behaviour.
