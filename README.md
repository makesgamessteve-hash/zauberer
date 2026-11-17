# Zauberer

*Working title. "Zauberer" is German for "wizard".*

Zauberer is an open-source game about a child who inherits a living flying carpet and a
murdered father's spellbooks. The child grows into a mage on an endless planet that is
generated lazily and simulated physically, chemically and biologically. Magic is built
from words, gestures, ingredients and eight colors of mana, and it acts through the
world's simulation of heat, fire, water, air and life.

> **Status: concept and planning.** There is no engine code yet. The first implementation
> milestone (M0, engine foundation) is fully specified and ready to start.

## Documents

| Document | What it covers |
| --- | --- |
| [Game concept](docs/concept/README.md) | Vision, pillars, story, carpet, magic, world generation, simulation, settlements, gameplay, presentation, modding |
| [Implementation plan](docs/implementation-plan.md) | Architecture, technology stack, repository layout, quality strategy, milestone roadmap M0–M10, decisions, risks, open questions |
| [OpenSpec change: engine foundation](openspec/changes/add-engine-foundation/proposal.md) | The first change (M0): proposal, design, nine capability specs, tasks |

## Technology

| Area | Choice |
| --- | --- |
| Engine | C11, SDL3, OpenGL 3.3 core / OpenGL ES 3.0 (WebGL 2) |
| Game layer | Lua 5.4 scripts and data, loaded from package folders next to the binary |
| Platforms | Windows, Linux, macOS, Web (Emscripten), WebXR; touch controls on mobile web |
| Quality | Spec-driven development with [OpenSpec](https://github.com/Fission-AI/OpenSpec), ≥ 95 % line coverage (C and Lua), automated headless game tests, CI on all platforms |

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

The license texts are in [`LICENSES/`](LICENSES/). Per-file license metadata (REUSE/SPDX)
and per-package asset credits are introduced with milestone M0.
