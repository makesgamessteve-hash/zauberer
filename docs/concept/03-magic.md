# 3. Magic

## 3.1 Principles

1. **Mana is energy with a flavor.** Every magical effect is paid for in mana, and mana
   comes in eight colors. Each color is tied to one physical domain of the simulation.
2. **Magic pushes the world; it rarely replaces it.** Moving, heating, cooling,
   accelerating or transforming matter that already exists is cheap. Creating matter from
   nothing is expensive. The player learns to use the environment: freeze a lake instead
   of conjuring ice, or ignite dry grass instead of conjuring a firestorm.
3. **All effects run through the simulation.** A spell produces sources and sinks of heat,
   matter, force, light, growth, decay or space folding. Everything after that, such as
   fire spread, steam, falling rocks or fleeing animals, is the simulation's response.
   There are no canned "burn" animations without an underlying heat source.
4. **Knowledge unlocks power.** The spell grammar is open: any combination can be tried.
   Knowing a combination well makes it cheap, safe and controllable. Not knowing it makes
   it costly and risky.
5. **Disciplines are methods; colors are forces.** The six disciplines (how you do magic)
   are orthogonal to the eight colors (what force you use). Fire Alchemy, Fire Summoning
   and Fire Ritual are all real and different.

## 3.2 Mana

### The eight colors

| Color | Name (in-world) | Physical domain and levers | Natural sources | Glyph |
| --- | --- | --- | --- | --- |
| Red | **Fire** (*Âtash*) | Heat, combustion, thermal radiation, melting | Volcanoes, wildfires, hot springs, sunlit deserts at noon, fire creatures | △ |
| Yellow | **Storm** (*Vâd*) | Gases, wind, pressure, electricity, kinetic motion | Thunderstorms, high altitudes, windswept cliffs, steppe | ⟁ (△ with bar) |
| White | **Light** (*Roshan*) | Light, perception, order, crystal lattices, binding | Clear skies, snowfields, crystal caves, holy sites, starlight | ○ |
| Green | **Life** (*Jân*) | Growth, metabolism, healing, fertility | Forests, jungles, meadows, coral reefs, dense animal life | ✚ |
| Blue | **Water** (*Âb*) | Liquids, solvation, cold, flow, erosion | Seas, rivers, lakes, rain, glaciers, mist | ▽ |
| Ochre | **Earth** (*Khâk*) | Solids, mass, structure, stone, metals, gravity | Mountains, caves, ore veins, deep bedrock | ⏚ (▽ with bar) |
| Violet | **Void** (*Khalâ*) | Space, distance, folding, illusion, mind, dreams | Night sky, auroras, ancient ruins, ley nexus, deep caverns | ◐ |
| Black | **Decay** (*Zavâl*) | Entropy, rot, fermentation, poison, endings | Swamps, fungal forests, carrion, battlefields, old graveyards | ✕ |

The in-world names are those of the protagonist's home culture, which are inspired by
Persian words for the classical elements. Other generated cultures name and value the
colors differently (see [World history](10-world-history.md)).

Each color also has a **distinct glyph and pattern**, so color-blind players never rely on
hue alone. The fire, water, air and earth glyphs come from the classical alchemical
symbols.

### The mana wheel

```
                 Fire
          Decay        Storm
       Void                Light
          Earth        Life
                 Water
```

- **Opposites** (across the wheel): Fire–Water, Storm–Earth, Light–Void, Life–Decay.
  Weaves that mix opposites are **volatile**. They are powerful and unstable, and they
  misfire easily. Examples: Fire + Water gives a steam blast, and Life + Decay gives a
  vitality transfer.
- **Neighbours** (adjacent on the wheel) are **harmonious** and combine cheaply. Examples:
  Fire + Storm gives lightning and explosions, Water + Life gives healing rain, Earth +
  Water gives mud and clay, Decay + Void gives curses and nightmares, and Light + Life
  gives restoration.
- **All other pairs** combine at normal cost and stability. An example is Earth + Fire,
  which gives lava.

### Mana as a field

Mana is a **field over the planet with eight channels**, one per color. It is sampled at a
coarse resolution (macro cell globally, about 32 m locally).

- **Emergent sources.** The local source rate of each color is computed from the physical
  state. Fire comes from temperature, active combustion and volcanism. Life comes from
  living biomass. Water comes from water volume and humidity. Decay comes from dead
  biomass and fungal activity. And so on. **Changing the world changes the mana.** A
  burned forest loses Life, gains a Fire spike, then turns to Decay as the ash and dead
  wood rot. Later it gains Life again as it regrows.
- **Reservoir and diffusion.** Each cell holds a stored amount up to a cap set by the
  source rate. Mana diffuses slowly between cells and flows fast along ley lines.
- **Depletion.** Drawing mana lowers the local reservoir. A depleted area is a
  "thin" zone: magic costs more, magical creatures leave, and mana-dependent plants wilt.
  Large rituals that draw too much cause **blight**, a long-lasting Decay surge. This is
  why Azhdar's path is visibly blighted.

### Ley lines and nexuses

- The global ley network is a procedurally generated graph on the macro grid. It follows
  geological fault lines, ridges and great rivers, and it is computed at world creation.
- **Nexuses** sit where several ley lines cross. They have very high mana, often of
  mixed colors, and they hold ancient structures, magical biomes and story sites.
- Ley lines are faintly visible to a trained eye: the player learns *Sense Void*, and
  attuned players see them shimmer at dusk.
- The world's ley network is **slowly weakening**. This is part of the plot and a slow
  world-state variable that endings change.

### Gathering mana

| Method | How | Notes |
| --- | --- | --- |
| **Drawing** | Meditate, or channel passively while still or flying slowly | Pulls from the local field; depletes it |
| **Motes** | Magical creatures, destroyed enchanted objects and mana blooms release glowing **motes** that drift and slowly dissolve back into the field | Collect them by flying through or with *Bind Void*; the core arcade loop |
| **Crystals** | Mineral deposits of crystallized mana in caves and at nexuses | Mined, stored and traded; used as artifact batteries |
| **Tinctures** | Alchemy distils mana from ingredients into liquid form | Portable, color-pure |
| **Domains** | A sanctum on a nexus collects mana continuously; servants bring motes | See 3.6 |

### The mage's mana

- **Reservoir:** one capacity for all colors combined. Mana keeps its color while stored,
  and the HUD shows it as a ring of colored segments.
- **Channeling rate:** how fast mana can be drawn in or poured out (this limits burst
  casting).
- **Attunement (per color, 0–100):** this lowers cost and improves control for that
  color. It grows with use, with time spent in places rich in that color, and with some
  foods and potions.
- **Strain:** pouring out mana faster than the channeling rate allows, or overcharging a
  weave, builds strain. Strain shows up as fatigue, blurred vision, temporary loss of
  control and, at the extreme, a short burnout. There is no instant death.

## 3.3 The spell grammar (weaving)

A weave is composed of five components:

| Component | Provided by | Defines |
| --- | --- | --- |
| **Words** | Spoken incantation (selected on a word wheel or bound to keys) | The *verb* (what happens) and *modifiers* (range, size, duration, delay, count) |
| **Gesture** | Drawn with mouse, finger, stick or XR controller | The *form*: the geometry of the effect |
| **Essence** | Mana of one or two colors from the reservoir | The physical domain |
| **Matter** | Optional ingredients from the belt or bag | Focus, catalyst, material supply, sympathetic link |
| **Power** | Charge time while holding the cast | Magnitude (how much mana is poured in) |

### Words: the eight verbs

Words come from the **Old Tongue**, a small constructed language whose words are learned
from books, teachers and inscriptions.

| Verb | Old Tongue (placeholder) | Meaning |
| --- | --- | --- |
| Create | *aen* | Add energy or matter of the color's domain |
| Unmake | *nehl* | Remove, negate or absorb it |
| Move | *vash* | Apply force or transport along the form |
| Shape | *tor* | Transform state or structure (phase, form, material) |
| Bind | *kel* | Make an effect persist, attach it to a thing or place, or hold something |
| Sense | *ith* | Reveal information |
| Ward | *ostra* | Protect against the color's domain |
| Call | *vo* | Summon a being of the color (Summoning discipline) |

**Modifiers** are additional words: *far*, *wide*, *lasting*, *many*, *delayed*, *upon
touch* and *returning*. Each one adds cost and complexity.

### Gestures: the eight forms

| Gesture | Form | Typical result |
| --- | --- | --- |
| Tap / point | Self or touch | Effect on caster or touched object |
| Line | Bolt or beam | Projectile or ray in the aim direction |
| Circle | Sphere or area | Effect centred on the aim point |
| Spiral | Vortex or sustained | Continuous channel while held |
| Triangle | Cone or burst | Fan-shaped spray from the hands |
| Square | Wall or barrier | Planar effect perpendicular to the aim |
| Zigzag | Chain or arc | Jumps between targets |
| Wave | Stream or spray | Flowing effect along a path |

Gesture **accuracy** (how well the drawn shape matches) adds to *control*. Sloppy
gestures cost more and misfire more often. An accessibility option offers gesture assist,
with simplified shapes and snapping.

### Verb × essence reference (the physical meaning of each combination)

| | Fire | Storm | Light | Life | Water | Earth | Void | Decay |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **Create** | Heat source, flame | Gust, static charge, spark | Light source | Growth surge, healing | Condense water from humidity | Conjure sand or stone (costly) | Fold a small pocket, blink anchor | Rot, fungal bloom |
| **Unmake** | Cool, extinguish (heat sink) | Still the air, ground lightning | Darkness | Wither | Dry, evaporate | Crumble, dig | Dispel, collapse folds | Purify, preserve, cure |
| **Move** | Hurl flame (fireball) | Wind blast, lift gases | Redirect or bend light | Animate vines, roots | Wave, water jet | Raise or lower terrain, hurl rocks | Short teleport | Spread rot along a path |
| **Shape** | Melt, smelt, temper | Condense a cloud, local weather | Illusion | Graft, reshape plants, close wounds | Freeze or thaw into shapes | Stone ↔ mud, sculpt walls | Enlarge or shrink a space | Ferment, decompose into components |
| **Bind** | Burning weapon, eternal flame | Store lightning in an object | Glyph, seal, light trap | Entangle, bond a familiar | Hold water in shape (bubble, wall) | Anchor, petrify | Trap in a pocket, bind a spirit | Lingering curse |
| **Sense** | Heat vision | Read the wind, foresee weather | Far sight, true sight | Detect life | Dowse for water | Detect ores and caves | Scry, see ley lines and magic | Detect poison, disease, death |
| **Ward** | Fire resistance | Deflect missiles, lightning rod | Barrier against Void and Decay | Regeneration | Cold and drowning protection | Stone skin | Conceal from scrying | Poison resistance |
| **Call** | Salamander, fire elemental | Wind sprite, storm bird | Wisp | Call beasts, dryad | Water elemental | Golem, earth elemental | Shade, phantom servant | Carrion swarm, fungal thrall |

Two-color weaves combine domains. Examples: Move + Fire/Storm gives an exploding fireball
with a pressure wave, Create + Water/Life gives healing rain, and Shape + Earth/Fire gives
a lava pool.

### Matter: ingredients

| Role | Effect | Examples |
| --- | --- | --- |
| **Focus** | Lowers cost and improves aim for a color | Amber (Storm), silver (Light), obsidian (Void), oak heartwood (Life) |
| **Catalyst** | Unlocks a variant | Sulfur makes fire burn hotter and smokier; salt makes water brine; quicksilver enables transmutation |
| **Material** | Supplies the matter for *Create*, cutting its cost sharply | Carry water for a water jet, carry sand for a sand wall |
| **Sympathetic link** | Targets something far away | A feather of a bird to scry its flock; a scrap of Azhdar's cloak to locate him |

Ingredients are consumed, which ties magic to gathering and alchemy.

### Power and cost

- Holding the cast pours mana in at the channeling rate. Releasing it fires the weave.
- Each color has a fixed **exchange rate** to physical quantities. For example, 1 mana of
  Fire ≈ 100 kJ of heat, 1 mana of Water ≈ 2 litres condensed from humid air, and 1 mana
  of Earth ≈ moving 1 t of rock by 1 m. The numbers are data in Lua and balanced later.
- **Effective cost** = base cost × form factor × modifiers × distance × environment
  factor × (1 − efficiency).
  - The **environment factor** comes from the local mana field of that color. Fire is cheap
    near a volcano and expensive in a blizzard. Water is cheap at the shore and
    expensive in the desert, where there is little humidity to condense.
  - **Efficiency** comes from the discipline skill, the color attunement and the
    familiarity with this specific weave.
  - **Matter from nothing** carries a large multiplier, so the cheapest path is always to
    use what the world offers.

### Prepared and free weaving

- **Free weaving** gives the full grammar: words + gesture + essence + matter. It is
  flexible, slower and needs skill. It is used for experimentation and special situations.
- **Prepared weaves** are known weaves inscribed in the spellbook. The player assigns them
  to the **left hand** and **right hand** slots and to a quick wheel of up to eight. Casting
  a prepared weave only needs the gesture, or a single button with gesture assist, plus
  charge. This is the fast mode for flying combat.
- **Discovery.** A free weave that matches a valid combination the player hasn't recorded
  is written into the journal automatically ("You discovered *Hold Water*"). Books and
  teachers reveal weaves directly. **Familiarity** with each weave rises with use, through
  the tiers *Unfamiliar → Practised → Known → Mastered*, and each tier improves cost and
  control and unlocks more modifiers.

### Misfires

Low control, overcharge, opposing essences or missing knowledge can cause a misfire.
Misfires are **simulated, not scripted**. The weave runs with a distorted form, wrong
target, partial essence or delayed release. Examples: a fireball that pops at the
caster's hand releases its heat right there, which singes the carpet. A lift weave that
overshoots throws the target into the air. Misfires hurt and teach, but they are never
instantly lethal.

## 3.4 The six disciplines

Each discipline has mastery tiers: *Novice → Apprentice → Journeyman → Adept → Master*.
Mastery grows through **practice** (doing the discipline), **study** (books, teachers,
experiments) and **trials** (a discipline-specific challenge to pass each tier, often tied
to a teacher or a story site).

### Spellcasting (weaving)

- **What:** real-time magic through the spell grammar above, used in flight, in combat and
  in exploration.
- **Where:** anywhere.
- **Progression:** more verbs, modifiers and two-color weaves; higher channeling rate;
  more hand slots and a larger quick wheel.

### Ritual magic

- **What:** slow, large-scale, durable magic. Rituals act on regions, not targets.
- **How:** the player draws a circle and sigils on the ground with a gesture at large
  scale, and places ingredients at the circle's points. Some rituals depend on timing:
  dawn, dusk, moon phase, season or weather. The player channels for seconds to minutes
  and is vulnerable while doing so. Summoned helpers or allies can add mana.
- **Examples:**
  - Terraforming: raise an island, carve a valley, call a spring, cool a lava field.
  - Weather: call rain over a region, dispel a storm.
  - Wards over territory: protect a campsite, a domain or a village.
  - Establish a **sanctum** (domain).
  - Gate between two attuned nexuses (fast travel).
  - Expand the tent with a new room.
  - Bind greater summons.
  - Long-range divination, such as locating a Great Weave or Azhdar.
- **Risk:** rituals draw a lot from the local field and can cause blight if overdone.
  Interrupted rituals backlash.

### Alchemy

- **What:** chemistry with a mana dimension, done in the tent laboratory with a simple
  field kit for basic steps.
- **How:** vessels hold mixtures, and apparatus applies processes: grinding, dissolving,
  heating, calcination, distillation, sublimation, filtration, crystallization,
  fermentation and amalgamation. The chemistry simulation decides what happens, based on
  temperature over time, proportions and catalysts (see [Simulation](05-simulation.md)).
- **Products:**
  - Potions: healing from Life-rich herbal extracts, warmth, night vision, water
    breathing, mana tinctures.
  - Poisons and antidotes.
  - Acids and solvents.
  - Fuels and incendiaries.
  - Dyes for the cloak.
  - Glass for apparatus.
  - Metals smelted from ores.
  - Preservatives.
  - **Quintessences:** pure single-color mana essences, used for artificing.
- **Knowledge:** substances start unidentified. Analysis in the lab or Codex study reveals
  their constituents. Recipes can be found, bought or discovered by experimenting.
- **Grounding:** real, era-plausible chemistry under historical names, such as saltpeter,
  vitriol, aqua fortis, quicklime, sal ammoniac and quicksilver. Mana adds reactions that
  wouldn't happen otherwise, such as transmutations that need a mana catalyst.

### Artificing (artifact creation)

- **What:** crafting physical objects and binding weaves into them.
- **Crafting base:** woodworking, sewing and weaving, simple smithing, glassblowing and
  carving. The workshop is in the tent, and a few tasks can be done in the field.
- **Enchanting** an artifact combines four parts:
  - a **base object**, whose material sets the **mana conductivity** per color (silver
    conducts Light, copper conducts Storm, oak conducts Life, obsidian conducts Void,
    bone conducts Decay),
  - one or more **bound weaves**,
  - a **power source**: an internal crystal, recharging from the wearer, or drawing from
    the ambient field,
  - a **trigger**: on use, on strike, passive, or a condition such as "when my
    temperature drops below 5 °C, warm me".
- **Examples:** wands (stored weaves), staves (amplify a color), rings and amulets
  (passives and wards), a lantern of perpetual light, a self-heating kettle, a compass
  that points toward a sympathetic link, boots of soft steps, a cartographer's quill that
  draws the map as you fly, and **golems** (with Summoning).
- **Upgrades** for the carpet, bag and tent are artificing projects.

### Summoning

- **What:** calling, binding and commanding beings.
- **Kinds:**
  - **Elementals** are formed from local matter and mana. A water elemental needs a body
    of water nearby, and a fire elemental must keep burning fuel or it fades. They obey
    the simulation, so a fire elemental can't cross a river.
  - **Spirits** (Void, Light) work as scouts, messengers and wisps for light.
  - **Familiars** are one bonded animal companion with traits that grow. Examples: an owl
    for night sight and scouting, a fox for stealth and foraging, a raven to fetch small
    items.
  - **Bound beasts** are wild creatures tamed through a contract, such as a griffon mount,
    a pack animal or a guard beast.
  - **Constructs** are golems from artificing.
  - **Servants** are small mana sprites that gather motes in a domain.
- **Costs:** a summoning cost, an upkeep per minute, and a binding strength. Weak binding
  means disobedience or escape. Contracts can be negotiated with intelligent beings.

### Naturalism (lore)

- **What:** knowledge of the living and mineral world. It is the foundation for alchemy,
  summoning and survival.
- **How:** observe (look at something for a while), sample (harvest correctly), study
  (dissect or analyse in the lab), read and ask.
- **Rewards:**
  - **Codex** entries that reveal properties: edible or poisonous, alchemical
    constituents, mana affinity, habitat, season, behaviour and weaknesses.
  - Better harvest yields through proper methods.
  - The ability to cultivate plants and to tame animals.

### Disciplines × colors (examples)

| | Fire | Water | Life | Void |
| --- | --- | --- | --- | --- |
| **Spellcasting** | Fireball | Water jet | Mend wounds | Blink |
| **Ritual** | Awaken a dormant volcano | Call rain | Regrow a burned forest | Gate between nexuses |
| **Alchemy** | Incendiary oil | Water-breathing draught | Healing salve | Dream tincture |
| **Artificing** | Flame-tongue staff | Ever-full waterskin | Ring of regeneration | Bag fold upgrade |
| **Summoning** | Salamander | Undine | Dryad, beasts | Shade servant |

## 3.5 Domains and rival mages

This is the homage to the territory game of the genre's roots, embedded in the open world.

- **Sanctum.** A ritual on a nexus raises a sanctum, a small tower that grows in tiers as
  it stores mana. The sanctum claims a **domain** around itself, roughly a few kilometres
  in radius depending on its tier.
- **Income.** The sanctum draws from the nexus. **Servant sprites** fly out, collect
  loose motes in the domain and carry them home. The player can follow them and protect
  them.
- **Benefits:**
  - a mana income that the player can transfer from any sanctum to themselves,
  - a gate anchor for fast travel between sanctums,
  - local wards,
  - a stage for big rituals,
  - local NPCs who may seek protection, which brings trade and quests.
- **Contest.** **Rival mages** run their own sanctums and servants. They raid,
  steal motes and attack sanctums, and they can be negotiated with. Destroying a rival
  sanctum releases its stored mana as a shower of motes.
- **Restraint.** Every domain draws on the world's weakening ley network. Too many domains
  thin the land. This feeds the central theme: power has an ecological price.
