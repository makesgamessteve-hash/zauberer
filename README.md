# Zauberer

<p align="center">
  <a href="https://makesgamessteve-hash.github.io/zauberer/"><b><font size="7">▶ PLAY</font></b></a>
  <br>
  <sub>In your browser: the latest web build, no install. Keyboard and mouse or touch.</sub>
</p>

*Working title. "Zauberer" is German for "wizard".*

Zauberer is an open-source game about a child who inherits a living flying carpet and a
murdered father's spellbooks. The child grows into a mage on an endless planet that is
generated lazily and simulated physically, chemically and biologically. Magic is built
from words, gestures, ingredients and eight colors of mana, and it acts through the
world's simulation of heat, fire, water, air and life. Play alone, or in online co-op
with up to three friends as the father's children, the heirs of the carpet.

> **Status: 1.0 release candidate (M11).** M0–M11 are done.
>
> - The engine runs on Linux, Windows, macOS and the web, with co-op flight over WebSockets
>   and a WebXR spike.
> - Worlds come from deep-time geology, climate, biomes, water bodies, vegetation, sky and
>   seasons, and are saved and continued.
> - The active tier simulates heat and fire, surface water, air, weather, rigid bodies and
>   mana fields.
> - Magic: an open weave grammar (words, gestures, colours, forms, modifiers, power) with a
>   spellbook, motes and channeling.
> - Living things: populations that migrate and virtualise, animals with needs, herds,
>   packs and scent tracking, plant life cycles, fungi and insects, survival (food, water,
>   rest, warmth, disease), foraging, hunting, campfires and cooking, and a Codex that
>   grows from Seen to Understood.
> - The carpet and home: the endless bag with its vault, the carpet's wear and repair and
>   upgrades, the carpet as a camouflage cloak, and the carpet as a tent with a pocket home.
> - The disciplines: chemistry and alchemy in the tent's laboratory, crafting and
>   artificing in its workshop (a lantern of perpetual light), ritual magic (calling rain),
>   summoning elementals and bonding familiars, and mastery through practice, study and
>   trials.
> - Places and people: the world is peopled (peoples, related invented languages, faiths,
>   realms, settlements, roads and a chronicle, or the optional Rise and fall simulation);
>   every house can be entered; citadels, caves with underground rivers and dungeons with
>   puzzles; people with their days, talk and trades; prices, factions and caravans; lore
>   told from each side. Vertical Slice B: the prologue in Anarvan and Act I (F5).
> - Together: up to four heirs join by invite code through a self-hostable relay or on the
>   LAN, ride the Weave together or split it, vote to sleep, and come back to the saved
>   world.
> - The story: after the prologue, a narrative director places the Circle's remnants in the
>   world's own history, and rumours lead to them. Act II has their revelations, rival mages
>   with domains and Azhdar's campaign of burning, blight and stolen Weaves. Act III has
>   divination, the ash storm and the duel with Azhdar. There are four endings, and the
>   world goes on after each of them.
> - Release: English and German, accessibility settings (remapping, toggle instead of hold,
>   gesture assist, colour glyphs and colour-vision palettes, text size and contrast, reduced
>   flashing, sound captions, story markers, survival presets), a menu on Escape, the game's
>   API for mods with an example mod, and release packages for every platform.

## Documents

| Document | What it covers |
| --- | --- |
| [Game concept](docs/concept/README.md) | Vision, pillars, story, carpet, magic, world generation, simulation, settlements, gameplay, presentation, modding |
| [Implementation plan](docs/implementation-plan.md) | Architecture, technology stack, repository layout, quality strategy, milestone roadmap M0–M11, decisions, risks, open questions |
| [OpenSpec specs](openspec/specs/) | The capability specs as they stand; every change that made them is in `openspec/changes/archive/` |
| [Lua API reference](docs/api/README.md) | Every function scripts can call (generated from the C bindings; type stubs in `docs/api/zauberer.d.lua`) |
| [Creating packages](docs/modding/packages.md) | Package layout, `package.lua`, scripts, hooks, data, languages, credits and testing a mod |
| [The game's API for mods](docs/modding/game-api.md) and [the example mod](docs/modding/example-mod.md) | `zauberer.add_item`, HUD lines, work every tick; `examples/lantern-moths` |
| [Changelog](CHANGELOG.md) | What changed in each release, starting with 1.0.0-rc.1 (milestones M0–M11) |
| [Cutting a release](docs/release.md) | Version, changelog, gates, desktop and web packages, signing, checksums, web deploy and tag |

## Technology

| Area | Choice |
| --- | --- |
| Engine | C11, SDL3, OpenGL 3.3 core / OpenGL ES 3.0 (WebGL 2) |
| Game layer | Lua 5.4 scripts and data, loaded from package folders next to the binary |
| Platforms | Windows, Linux, macOS, Web (Emscripten), WebXR; touch controls on mobile web; cross-play co-op for up to four players |
| Quality | Spec-driven development with [OpenSpec](https://github.com/Fission-AI/OpenSpec), ≥ 95 % line coverage (C and Lua), automated headless game tests, CI on all platforms |

## Playing

The game starts on the carpet over a new world. Default bindings (keyboard and mouse;
gamepad and touch bindings are in `assets/base/data/input.lua`):

| Action | Keys |
| --- | --- |
| Fly | W/S speed, A/D turn, R/F climb and sink, Shift boost, mouse looks around |
| Cast | hold the left or right mouse button to charge the hand's weave, let go to cast |
| Draw a gesture | hold G and draw with the mouse (while charging: better control) |
| Free weaving | Q picks the verb, Z the colour, X a second colour, then draw a form and cast with the left hand |
| Quick wheel | 1–8 put a prepared weave in the right hand |
| Channel mana | hold V while still |
| On foot | E steps off a carpet resting low and slow (and back on); Space jumps; Shift runs |
| Use | Tab opens doors and chests, takes what is in them and talks with people (Shift+Tab asks for their service) |
| Story | F5 begins the prologue in Anarvan (Vertical Slice B); Acts II and III follow on their own; when Azhdar falls, F5 shows the endings and Tab takes one |
| Co-op | F6 splits the Weave into a carpet each or rejoins it; L in company opens or agrees to a vote to sleep |
| Menu | Esc opens the menu: settings (language, accessibility), controls (remapping), packages (mods), credits, save and quit |
| Other | N new world, F1 credits, F11 fullscreen |

Co-op: run the relay (`zauberer-relay --port 47821`, built next to the game) where every
player can reach it; the host calls `net.host_relay("ws://<relay>:47821")` and gives the
others its invite code (in the browser the relay must be behind a TLS proxy, `wss://`).

Vertical Slice A, the M4 demo: scare the wolves near the start with fire, put out the
grass fire with Unmake Fire, bring down the storm bird with a Fireball and collect its
motes, then freeze deep water and walk out onto the ice.

## Building and running

Requirements: CMake ≥ 3.25, Ninja, a C11 compiler (GCC, Clang or MSVC) and Python 3.
SDL 3 is fetched at a pinned commit; Lua, stb, cgltf and Unity are vendored in
`third_party/`. On Linux, SDL needs the usual development headers (X11 or Wayland, EGL,
ALSA or PulseAudio).

```sh
cmake --preset dev                 # debug build with hot reload and tests
cmake --build --preset dev
build/dev/bin/zauberer             # the demo (assets/ is staged next to the binary)
```

Presets: `dev`, `release`, `coverage` (GCC/Clang + gcovr), `asan` (AddressSanitizer and
UBSan), and with the [Emscripten SDK](https://emscripten.org) (version in
`cmake/emsdk-version.txt`, `EMSDK` set): `web` (threads, needs cross-origin isolation),
`web-st` (single-threaded) and `web-node` (headless engine and unit tests under Node.js).

```sh
source "$EMSDK/emsdk_env.sh"
cmake --preset web-st && cmake --build --preset web-st
node tests/web/server.mjs build/web-st/bin    # prints the URL to open
```

With a WebXR headset (or a WebXR emulator extension), the page shows an **Enter VR** button
that starts an immersive session; the camera becomes the seat the head moves around.

Release packages (one plain folder per platform with the game, the relay, `assets/`,
`mods/`, licenses and credits) come from `python3 tools/release.py package`; see
[docs/release.md](docs/release.md).

Useful options: `--assets <dir>`, `--user-dir <dir>`, `--headless`, `--ticks <n>`,
`--seed <n>`, `--debug-views 2` (stereo), `--record <file>` / `--replay <file>`
(`zauberer --help` lists all). Player data lives in `%APPDATA%/Zauberer`,
`$XDG_DATA_HOME/zauberer`, `~/Library/Application Support/Zauberer` or IndexedDB on the web.

Co-op: `zauberer --host 7777` hosts a session on a desktop and `zauberer --join
ws://<host>:7777` joins it, natively or in a browser (`?args=--join%20ws://localhost:7777`).
Up to four heirs fly together; every game generates the same world from the seed.

## Testing

```sh
ctest --preset dev                                   # unit, engine, rendered and tool tests
build/dev/bin/zauberer --headless --test tests/lua --junit lua.xml
build/dev/bin/zauberer --headless --scenario tests/scenarios/demo_runs.lua
build/dev/bin/zauberer --headless --replay tests/replays/demo.zbrec   # checks recorded state hashes
cd tests/web && npm install && node --test web.test.mjs              # browser tests (web, web-st builds)
```

- **Unit tests** (Unity, `tests/unit`) check every C module and fail on leaked allocations.
- **Engine tests** (`tests/engine`, Python) drive the binary: startup errors, scenarios,
  Lua tests, replays and, on Linux, rendered golden images (SDL offscreen driver + Mesa
  llvmpipe). Regenerate goldens with `ZB_UPDATE_GOLDEN=1`.
- **Quality gates** (`tools/`): line coverage ≥ 95 % for C and Lua with a ratchet
  (`coverage_gate.py`), every spec scenario tagged by a test (`spec_coverage.py`), module
  layering (`check_includes.py`), REUSE/license split (`check_licenses.py`, `reuse lint`),
  asset credits (`check_credits.py`) and the generated Lua API reference
  (`gen_api_docs.py --check`). CI runs all of them
  ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)); for now it is started by
  hand to save build minutes, and the full gates run locally once per milestone.

## Working with OpenSpec

This repository uses OpenSpec's `spec-driven` workflow. The project context and rules are
in [`openspec/config.yaml`](openspec/config.yaml). The OpenSpec skills for Claude Code are
in `.claude/skills/`.

```sh
npm install -g @fission-ai/openspec   # or use npx
openspec list                          # changes in progress
openspec show add-engine-foundation    # read a change
openspec validate --all --strict       # validate specs and changes
```

To implement a change, use `/openspec-apply-change` in Claude Code. To start a new
milestone, use `/openspec-propose`.

## License

- **Code** (engine, scripts, shaders, build files and tools) is licensed under the
  [MIT License](LICENSE).
- **Art and other creative assets** (textures, sprites, models, voxels, audio, music,
  project fonts, story text) and the documentation are licensed under
  [Creative Commons Attribution 4.0 International (CC BY 4.0)](LICENSES/CC-BY-4.0.txt).

The license texts are in [`LICENSES/`](LICENSES/). Every file carries SPDX metadata (headers
or [`REUSE.toml`](REUSE.toml)); every package credits its assets in `credits.lua`.
Third-party code is listed in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
