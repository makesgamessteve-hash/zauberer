# Zauberer: Game Concept

> Working title. "Zauberer" is German for *wizard* (and *sorcerer*). Every name in these
> documents is a placeholder until the story is locked.

## One-line pitch

A weaver's child survives the destruction of their oasis village, inherits a living
flying carpet and a father's secret spellbooks, and grows into a powerful mage. The
setting is an endless, simulated planet in the spirit of the *Thousand and One Nights*
and the Silk Road. Fire spreads, water flows, forests grow and rot, and every spell
pushes on the real physics, chemistry and biology of the world.

## The game in one paragraph

You fly a carpet over a whole planet that is generated lazily, chunk by chunk, as you
explore it. The planet has continents, oceans, mountain ranges, deserts, rainforests,
tundra and magical biomes. All of it is the result of a simulated geological history and
a consistent model of climate and hydrology. Peoples, languages, faiths, realms and ruins
come from a reconstructed or fully simulated human history. Thick sprite forests hide animals, insects and fungi that live,
hunt, migrate and die whether you watch or not. Villages, towns, castles and ruins are real
places that you can enter, room by room. Magic is built from **words, gestures, ingredients
and colored mana**, and it acts *through* the simulation. A fireball adds heat that ignites
dry grass, the wind carries the fire, the fire makes smoke, rain puts it out, and the burnt
land regrows over the next seasons. You survive, gather, learn, craft, brew, enchant,
summon and perform rituals, all to grow strong enough to face the mage who murdered your
family.

## Design pillars

1. **A world that exists without you.** The planet is consistent from the global climate
   down to a mushroom on a rotting log. Things happen off-screen at reduced detail and are
   caught up when you return.
2. **A world with a past.** Mountains come from colliding plates, deserts from rain
   shadows, ruins from real wars, place names from dead languages, and faiths from
   remembered disasters. Everything has a cause that the player can uncover.
3. **Magic is physics with intent.** Spells never trigger canned effects. They inject heat,
   matter, force, growth, decay, light or folded space into the simulation, and the world
   reacts on its own.
4. **Knowledge is power.** Progression comes from learning: spell formulae, the properties
   of plants and minerals, creatures' habits, recipes and rituals. A level-up screen is not
   the main source of growth.
5. **The carpet is home.** The carpet is your vehicle, your cloak and your house: a tent
   that is far bigger inside than outside. It is the emotional anchor, the last thing left
   of your family.
6. **Endless but meaningful.** Procedural content is generated in layers, from planet to
   region to chunk to room. The story is anchored into that procedural world, so every
   world has its own geography for the same tale.
7. **Retro look, modern depth.** Low-poly 3D, pixel textures, billboard sprites and
   optional voxels, rendered at a low internal resolution with fog and a limited palette.
   The style keeps the scope achievable and lets the systems carry the experience.
8. **Open and moddable.** A C core engine and a Lua game layer. All assets, scripts and data
   sit in plain folders next to the executable. Code is MIT, art is CC BY 4.0.

## Inspiration, not recreation

The genre roots are the mid-90s flying-carpet mage games. Bullfrog's *Magic Carpet* (1994)
and *Magic Carpet 2* (1995) gave us the carpet as a vehicle, collecting mana, reshaping
terrain with spells, and rival wizards competing over territory. Zauberer borrows those
**mechanics and that fantasy**. It uses its own title, world, characters, story, art, music
and level design. No names, assets, levels or visual identity from those games are used.

The **setting is intentionally Oriental**, because that is where flying carpets belong.
It draws on the *Thousand and One Nights*, Persian carpet weaving and miniature painting,
and the cultures of the Silk Road from North Africa to Central and South Asia. The
inspiration comes from real history and craft, with fictional peoples and faiths and no
caricatures (see [World history §10.4](10-world-history.md#104-phase-3-peopling-common-foundations)).
Other influences on systems include the falling-sand and systemic games (emergent fire and
fluids), survival-crafting games (gathering and base building), Dwarf-Fortress-style world
histories, and voxel sandbox games (endless chunked worlds).

## Platforms and technology at a glance

| Aspect | Decision |
| --- | --- |
| Engine | Custom, C11, SDL3, OpenGL 3.3 core / OpenGL ES 3.0 (WebGL 2) common subset |
| Game layer | Lua 5.4 scripts and data, loaded from folders next to the binary |
| Targets | Windows, Linux, macOS, Web (Emscripten), WebXR (immersive VR in the browser) |
| Input | Keyboard/mouse, gamepad, touch (auto-selected on mobile web), XR controllers |
| Quality | OpenSpec-driven development, ≥ 95 % line coverage, automated headless game tests |
| Licenses | Code: MIT. Art, audio and other creative assets: CC BY 4.0 |

See [the implementation plan](../implementation-plan.md) for architecture and roadmap.

## Concept documents

| # | Document | Contents |
| --- | --- | --- |
| 1 | [Story and setting](01-story.md) | Protagonist, prologue, the Ash Weaver, acts, endings, procedural story anchoring |
| 2 | [Carpet, bag and tent](02-carpet-bag-tent.md) | Flight model, cloak mode, tent mode and the pocket home, the endless bag |
| 3 | [Magic](03-magic.md) | Mana colors and fields, ley lines, the spell grammar, the six disciplines, domains |
| 4 | [World generation](04-world-generation.md) | Planet model, generation pyramid, terrain, geology, climate, biomes, lazy streaming |
| 5 | [Simulation](05-simulation.md) | Materials, heat, fire, fluids, gases, chemistry, ecology, simulation tiers |
| 6 | [Settlements and architecture](06-settlements-architecture.md) | World history, settlements, enterable buildings and castles, dungeons, NPCs |
| 7 | [Gameplay](07-gameplay.md) | Core loops, survival, knowledge, progression, combat, rival mages, saving |
| 8 | [Presentation and controls](08-presentation-controls.md) | Art direction, sprite forests, audio, UI, desktop/gamepad/touch/XR controls |
| 9 | [Extensibility](09-extensibility.md) | Folder layout, packages and mods, Lua API surface, data registries, licensing |
| 10 | [World history](10-world-history.md) | Deep-time geology, Earth-likeness, peopling (*Settled world* or simulated *Rise and fall*), cultures, languages, faiths, polities, lore |
| 11 | [Co-op](11-co-op.md) | Up to four heirs, splitting the Weave, shared and personal progress, sessions, cross-play, engine implications |

## Scope boundaries

**In scope for 1.0:**

- single-player and **online co-op for up to four heirs**, with cross-play between all
  platforms,
- one planet per save, deep-time world creation with both history variants,
- the full story, all six disciplines, the eight mana colors and the carpet's three modes,
- enterable settlements and citadels, caves and dungeons, and rival mages,
- Windows, Linux, macOS, Web and WebXR, and mod support.

**Out of scope for 1.0:**

- native Android and iOS apps (planned after 1.0; mobile is served by the web build until
  then),
- native OpenXR on desktop,
- split-screen co-op, more than four players, and host migration,
- right-to-left languages,
- multiple planets.

## Glossary

| Term | Meaning |
| --- | --- |
| **Chunk** | 256 m × 256 m square of the planet surface; the unit of streaming and saving |
| **Region** | 16 × 16 chunks (≈ 4 km); the unit for planning settlements, roads and rivers |
| **Macro cell** | ≈ 6 km cell of the global planet model (climate, plates, biomes, history) |
| **Mana** | The energy of magic. It comes in eight colors and is present as a field everywhere |
| **Ley line / nexus** | Channels of strong mana flow across the planet, and their crossing points |
| **Weave** | A composed spell (words + gesture + mana + ingredients); also, the in-world word for any magical structure |
| **Great Weave** | One of seven legendary artifacts; the carpet is one of them |
| **Domain** | A territory claimed by a mage through a sanctum built on a nexus |
| **Simulation tier** | Level of detail at which a place is simulated (active, near, far, dormant, global) |
| **Package** | A folder of assets, data and scripts (the base game is itself a package; mods are packages) |
| **Heir** | A player character: one of Farrokh's children. One to four heirs share a world |
| **Host** | The player whose machine runs the authoritative simulation and keeps the save in co-op |
| **World bundle** | The stored result of world creation (planet fields, rock record, ley graph, history database), saved with each world |
| **Style family** | A library of architecture, ornament, textile, music and naming vocabulary that generated cultures inherit and blend |
| **Settled world / Rise and fall** | The two peopling variants: direct placement with a reconstructed chronicle (default), or a full simulation of societies (optional) |
