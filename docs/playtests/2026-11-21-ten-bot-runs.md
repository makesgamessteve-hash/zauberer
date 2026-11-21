<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# Ten bot playthroughs (2026-11-21)

A gameplay bot (`tests/bots/play_bot.lua`, run by `tools/run_bots.py`) played ten
fresh games on the release build: worlds 1–10, 150 000 ticks each (about 25 game
hours). It plays the way a player reading the HUD would: walking, flying, talking,
eating and sleeping, and following the story goal. It rotates through four styles:
story, explorer, hunter and camper. The raw logs are in
[2026-11-21-logs/](2026-11-21-logs/).

## The short story

*The Heir of Anarvan, as ten heirs lived it*

Every morning starts the same way. The heir wakes by Farrokh's door, takes the jar to
the qanat and fills it. The neighbours are friendly, but not to them. "You are not
from here," they say, or "Keep your hands where I can see them," to the child who
grew up in the next house. Then they tell the old stories anyway: the faithless men
of Sohtashta, the Pass of Sorrow, the Weeping Bridge, the dead lying in thousands,
and a song about Chaana, who led them to victory. Shotnagar is a day's ride east.
Sekheho is building an observatory, they say. The goats crowd round for their fodder.

Then the heir sets out for the orchard, and the day goes wrong.

Seven of the ten never get there. They walk into a wall eighty-five metres short,
turn back and try again until nightfall. Meanwhile they empty the neighbours'
chests (dried meat, flatbread, a few coins), and no one minds. Overhead, storm birds
circle. Twice the storms bring lightning. It strikes the dry grass sixty metres away,
the fire runs through the village, and an heir dies in it, gets up in the same
flames and dies again: five times in one afternoon, and once more at the weaver's
house after dark. A third heir meets a grey wolf at the edge of the village. It
brings them down ninety-three times in one night, and every time they get up beside
it.

Three heirs pick the pomegranates. All three then spend hours pressed against the
wall of their own father's house, and one is still there the next morning. Two get
in. They find Farrokh at his loom and watch him light the lamp with a word, "a
weaver's sleight of hand."

For those two the story finally moves. Dusk comes, and a dark carpet comes down over
the square. Azhdar cries "Give me the Weave!" and no one understands him. Farrokh
presses a plain rug into the heir's arms and says, "Hide." One heir never finds the
way down through the hatch and wanders the dark house until morning. The other gets
down, and the hatch closes over them. Through the cracks: fire over the
roofs, screams, Farrokh's magic flaring blue and going out, Azhdar burning carpet
after carpet, asking, "Where is it?" The ward does not hold the heat. The heir burns
four times in the dark before the hoofbeats fade into silence.

Morning. Anarvan is ash. Going back to the ruins, the heir burns on the embers again
and again. They go down again and find the cellar larger than a cellar should be. On
its shelves are Farrokh's Primer (Kindle, Light, Lift), the endless bag, and a
letter in his hand that breaks off. The heir lies down on the warm rug and sleeps
eight hours. In a dream the carpet chooses them. It stirs under their hands.

And then it will not take them. Hungry, thirsty and alone in the ashes, the heir
tries thirty times to climb on, and the carpet stays on the ground. That is where
the luckiest heir's story ends, and no heir got further: the valley never
left behind, the rings of the first flight never flown.

## What happened in each run

| Run | Style | Furthest beat | Defeated | What happened |
|---|---|---|---|---|
| 1 | story | village | 1 (fire) | Water and goats done; talked to Farrokh. The orchard was never reached (85 m short, 9 tries). Looted five chests. Burnt in a grass fire at the weaver's house at 20:33. Spoke with 15 villagers. |
| 2 | explorer | village | 0 | Flew 1.9 km out and back first. Water and goats done; the orchard was never reached (8 tries). Ran out of water by night 2. |
| 3 | hunter | handover | 0 | Water, goats and pomegranates done. Spent 5 h reaching Farrokh ("Keep your hands where I can see them"). Arrival and handover played out, then 17 h stuck at the cellar hatch or stair, 1–5 m short. |
| 4 | camper | village | 0 | Flew 2 km out and back. Water and goats done; the orchard was never reached (88 m short, "inside a building"). Ran out of food and water. |
| 5 | story | village | 5 (fire) | Water and goats done. Lightning struck 62 m away and lit the grass. The heir was revived in the same fire four times within 45 minutes. |
| 6 | explorer | **flying** | 10 (9 fire, 1 fall) | All four chores, Farrokh, arrival, handover and hiding. Burnt 4 times inside the warded cellar while the village burned, then 5 times on the embers in the morning (and one fall). Found the Primer, the bag and the letter; slept and bonded with the carpet. Then "the carpet would not take the heir" (30 tries). Ended with food 0, water 6. |
| 7 | hunter | village | 0 | Water done. The goat pen was never reached (41 m short, "inside a building") for 20 h. |
| 8 | camper | village | 93 (wolf) + 2 (hunger) | Water and goats done. A grey wolf by the orchard path bit the heir down 93 times in about 20 h. Revived at about 40 hp beside it each time. |
| 9 | story | village | 0 | Water and goats done; the orchard was never reached (7 tries). Hunger at the end. |
| 10 | explorer | village | 0 | Flew about 7.6 km and found a shrine. Water, goats and pomegranates done; Farrokh was never reached (about 60 m short, against a wall) for 20 h. |

Across all runs the codex filled with 6–13 entries: grass, acacia, red deer,
fern, palm, hare, bee, butterfly, storm bird, fox, boar and wolf.

## What the runs show

These are findings to check by playing. Some may be the bot's walking rather than
the game.

1. **Lightning fires kill and keep killing.** A storm's lightning lights grass
   40–300 m from the player (`src/sim/tier.c` `weather_second`). The heir is revived
   in place, still in the flames. There is no warning, and nothing steers the player
   out of the fire. (Runs 1, 5.)
2. **Being revived next to the danger.** After a defeat, the heir gets up on the
   same spot at about 40 hp, next to the same wolf or fire, so a defeat loops. (Runs
   5, 8: 93 defeats.)
3. **The warded cellar lets the heat in.** During *hiding*, the heir takes heat
   damage from the burning houses above. On the morning of *ashes*, the embers
   defeat them again. (Run 6.)
4. **The carpet refuses after the bond.** In *flying*, mounting failed 30 times,
   so the prologue cannot be finished. The cause is not yet known. (Run 6.)
5. **The story places are hard to reach.** The orchard (7 of 10 runs, about 85 m
   short), the goat pen (run 7), Farrokh inside his house (runs 3, 10) and the
   cellar hatch (run 3) stopped the bot for hours. A human may do better, but there
   is no marker or path to these places, and the orchard target may sit inside a
   walled plot.
6. **Your own village treats you as a stranger.** Villagers, and Farrokh himself,
   greet the heir with "Good day, traveller", "You are not from here" or "Keep your
   hands where I can see them".
7. **Taking from neighbours costs nothing.** Chests in other people's houses can be
   emptied with no one reacting.
8. **The prologue is slow.** One morning of chores took the bot 3–11 game hours.
   At 36× time, that is several real minutes of walking per chore.
