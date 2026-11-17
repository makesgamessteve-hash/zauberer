# 1. Story and Setting

## 1.1 Setting

A temperate valley on an Earth-like planet in a pre-industrial, low-magic age. Most people
are farmers, herders, hunters and craftsfolk. Mages are rare, often feared, and usually
work alone. Magic is real and powerful, but the ley lines that carry mana have weakened for
generations. Few people know this, and fewer care.

Long ago, seven mages formed the **Circle of the Loom**. Together they wove the seven
**Great Weaves**, artifacts that each bind a strand of the planet's ley network. The
flying carpet is one of them: the *Wandering Weave*. The Circle broke apart. Its members
scattered, hid their Weaves, and most of them grew old in obscurity.

The player's father, **Joren**, was the youngest member of the Circle. He settled in the
village of **Haselbrunn**, married, raised a child, and hid the carpet in plain sight as
the rug on his living-room floor. He hid his books and tools in a sealed cellar beneath
the house.

## 1.2 The protagonist

- The player chooses a girl or a boy (default names *Elin* / *Tavi*, renameable), about
  11 years old at the start.
- The protagonist is physically weak. They can't wear armour or win a sword fight. Their
  strength is curiosity, stealth and a growing talent for magic.
- **Optional aging:** game time passes in seasons and years. The protagonist ages visibly
  (11 to roughly 15 over a typical playthrough). Aging gives small physical gains such as
  carry weight and stamina, and changes how NPCs speak to them. It is cosmetic-plus, never
  a fail state.

## 1.3 Prologue: "Haselbrunn" (tutorial, 20–40 minutes)

1. **Village life.** The player wakes up, does chores for their father (fetch water, feed
   the goats, pick herbs) and talks to villagers. This teaches walking, interacting,
   picking things up and the basic inventory. Joren shows small "tricks", such as lighting
   a candle with a word. He passes them off as sleight of hand.
2. **The arrival.** In the evening, a mage on a dark carpet descends on the village. This
   is **Sereth Vane, the Ash Weaver**. He demands "the Weave", and the villagers don't
   understand. The tension rises.
3. **The handover.** Joren rolls up the living-room rug, presses it into the child's arms
   with a small leather bag, and says: *"Hide. Don't come out, whatever you hear."* He
   opens the hidden cellar hatch and closes it over the child.
4. **The hiding** (a short, tense stealth section with no fighting possible). Through
   cracks in the floor the player hears and sees fragments: fire, screams, Joren's magic
   flaring briefly. Sereth's search turns the village into ash. Sereth senses that the
   Weave is close but can't find it, because the cellar is warded. He leaves in fury.
5. **Ashes.** Morning comes and the player climbs out. The village is destroyed and
   everyone is dead. There is an optional memorial: the player can bury their father and
   light a candle. This is the first "ritual" of the game and it unlocks the first
   journal entry.

## 1.4 Act I: "The Hidden Room" (early game)

- **The father's sanctum.** The cellar is larger than a cellar should be. Its first
  room holds Joren's spellbooks, alchemical tools, notes and the **endless bag**. A letter
  addressed to the child explains, sparingly, what Joren was. The letter is incomplete and
  becomes a thread for later discoveries.
- **First magic.** From the *Primer* (Joren's first spellbook), the player learns the
  basics of the spell grammar: *Kindle* (a small heat source), *Light* and *Lift*.
- **The carpet wakes.** A ritual of bonding: the child sleeps on the carpet. In a dream,
  the carpet "chooses" them. Afterwards the carpet responds, first by hovering, then by
  short flights.
- **Learning to fly.** This is a guided flying tutorial over the valley.
- **Survival begins.** Food stores are burnt. The player forages, sets a first snare and
  boils water.
- **Leaving.** With the carpet, the bag and a few books, the player leaves the valley to
  follow the trail of scorched earth that Sereth left behind.

## 1.5 Act II: "The Wide World" (the long middle, mostly open-ended)

This act is a sandbox with story threads. The player can follow it or ignore it for hours.

- **The trail of ash.** Sereth hunts the other Great Weaves. He destroys settlements as he
  goes, and these appear as burnt, blighted places on the world map. The player finds
  survivors, rumours and clues.
- **The Circle's legacy.** The player tracks down the remaining Circle members or their
  graves, students and towers. Each one can:
  - teach a discipline, such as alchemy from the old herbalist in the swamp or summoning
    from the hermit in the mountains,
  - reveal part of the backstory,
  - hold a Great Weave, which the player can secure first, or lose to Sereth if the player
    arrives after him.
- **Rival mages.** Independent mages roam and claim domains. Some are hostile, some trade
  and teach, and some serve Sereth (see [Gameplay](07-gameplay.md)).
- **Revelations.**
  - Sereth was Joren's closest friend and fellow Circle member.
  - The ley network is slowly dying, and magic itself is fading from the world.
  - Sereth believes that only a *reweaving* can save magic: gathering all seven Weaves and
    binding the ley network to one will, his own. He is willing to burn the world's people
    to save the world's magic.
  - Joren believed the Weaves had to stay apart, because one mage controlling all mana
    would be worse than a slow fading.

## 1.6 Act III: "The Reweaving" (endgame)

- Sereth's citadel is a fortress that drifts on a ley storm. Its position is revealed by
  divination and by the Weaves the player holds.
- To reach it, the player needs mastery in several disciplines. Examples: a ritual to
  calm the storm, artifacts that protect against Sereth's ash magic, and summoned allies.
- The final confrontation is a duel that uses the whole spell grammar and the environment
  (the citadel has its own simulated materials, fires, winds and mana fields).

### Endings (the player chooses)

| Ending | Outcome | World afterwards |
| --- | --- | --- |
| **Unravel** | Destroy Sereth and unravel all Weaves | Mana slowly fades over the following game years. Magic gets more expensive but people live freely |
| **Bind** | Imprison Sereth inside his own Weave | The Weaves stay scattered. Status quo, with a prisoner who may whisper |
| **Reweave** | Perform the reweaving yourself, but share it (requires deep Ritual and Void mastery) | Ley lines are restored. The player becomes a guardian, and rival mages must be dealt with |
| **Succession** | Take Sereth's place | The player becomes the new tyrant of mana. A dark ending, and the sandbox continues with the world reacting |

After any ending the world continues as an endless sandbox, with consequences applied to
mana fields, factions and NPC attitudes.

## 1.7 How the story lives in an endless procedural world

The story is authored as a **graph of beats** in Lua. A **narrative director** anchors the
beats into the generated planet:

- **Beats, not places.** A beat declares requirements, for example "a hermit's tower in a
  mountain biome, 30–80 km from the player, not yet visited, near a ley nexus". The
  director queries the world's region index for candidate sites that match, picks one
  deterministically from the world seed, and reserves it. The region generator then places
  the authored *set piece* there, which is a parametric structure template.
- **Pacing.** Sereth's campaign (villages burnt, Weaves taken) runs in the global macro
  simulation on a timeline that is paced by player progress. Being slow never makes the
  story fail. The world just gets darker: more ruins, more blight, more refugees on the
  roads.
- **Rumours.** NPCs, books and divination point the player toward the next beats with
  direction and distance ("far to the north-east, past the great river"). There are no
  quest markers by default. An accessibility option can enable them.
- **Determinism.** The same seed and the same choices give the same anchoring, so story
  problems are reproducible in automated tests.

## 1.8 Tone

The tone is melancholic and wondrous, like a dark fairy tale that is mostly about wonder.
Grief is real but not grim. The world is beautiful, dangerous and indifferent, and kindness
in it matters. Violence against people happens off-screen or at a distance in the
prologue. The retro art style helps keep it tasteful.
