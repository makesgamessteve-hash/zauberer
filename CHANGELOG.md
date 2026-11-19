<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# Changelog

All notable changes to Zauberer. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/). The release version has one source,
`ZB_VERSION` in `src/core/version.h`; how to cut a release is in
[docs/release.md](docs/release.md).

## [1.0.0-rc.1] - 2026-11-19

The first release candidate for 1.0: the whole roadmap (docs/implementation-plan.md §5)
from the engine foundation to the story, packaged for Linux, Windows, macOS and the web.

### M0: Foundation

- A C11 engine on SDL3 and OpenGL 3.3 / WebGL 2 for Linux, Windows, macOS and the web,
  with the base game as a Lua 5.4 package in `assets/base` and mods in `mods/`.
- Headless runs, scenarios, Lua tests and replays; coverage, sanitizer, license and
  spec-coverage gates from the first day.

### M1: First Flight

- Endless flight over a cube-sphere planet with streamed terrain chunks and the retro
  renderer (internal resolution, palette, dither, fog, curvature).
- Touch and gamepad controls, a WebXR stereo spike, the network foundation (two carpets
  over WebSockets) and the procedural art generators.

### M2: A Planet

- Deep-time geology, climate and biomes, world creation with a time-lapse globe, oceans,
  rivers and lakes, the sprite forest, sky, seasons, and saving and loading worlds.

### M3: Living Physics

- Materials, heat and fire, shallow water, wind, smoke and scent, weather with
  lightning, simulation tiers with catch-up, terrain deformation and rigid bodies.

### M4: First Magic and Vertical Slice A

- Mana fields and ley lines, the weave grammar, gesture recognition, the spellbook, mana
  motes, the first creatures and the damage model; the 15-minute Vertical Slice A.

### M5: Living Things

- Ecology with migrating, virtualised populations, herds, packs and scent tracking, plant
  life cycles, fungi and insects, survival needs and disease, harvesting and cooking, and
  the Codex.

### M6: Carpet and Home

- The endless bag and its vault, the carpet as a camouflage cloak and as a tent with a
  pocket home, and the carpet's wear, repair and upgrades.

### M7: The Disciplines

- Chemistry and the alchemy laboratory, crafting and artificing, ritual magic, summoning
  elementals and bonding familiars, and mastery through practice, study and trials.

### M8: Places and People and Vertical Slice B

- Peoples with languages, faiths, realms and a chronicle (or the Rise and fall history
  simulation), lore told from each side, settlements with enterable buildings, citadels,
  caves and dungeons, NPCs, factions and the economy; the prologue and Act I.

### M9: Together

- Co-op for up to four heirs by invite code through the self-hostable `zauberer-relay`
  or on the LAN, riding the Weave together or split, votes to sleep, several active
  areas, and hardening against latency, loss and reconnects.

### M10: The Story

- The narrative director, Acts II and III with the Circle's remnants, rival mages and
  their domains, Azhdar's campaign, the duel, four endings and the world after them.

### M11: Release

- One version source (`src/core/version.h`) checked against `assets/base/package.lua`.
- Store-agnostic packages: CMake install rules and CPack archives per desktop platform
  (`zauberer-<version>-<os>-<arch>.tar.gz` / `.zip`) with the shipped layout, licenses,
  third-party notices and generated credits; a zip of the web build.
- Optional signing (signtool, codesign, GPG) when keys are configured, and `SHA256SUMS`.
- `tools/release.py` (check, package, web, sums), `tools/deploy_web.py` (the web build to
  the `gh-pages` branch), a manual release workflow and [docs/release.md](docs/release.md).
