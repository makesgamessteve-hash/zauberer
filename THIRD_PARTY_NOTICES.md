<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# Third-party notices

Zauberer includes or builds with the following third-party components. Their license
texts are in the listed files; the SPDX metadata is in [`REUSE.toml`](REUSE.toml).

| Component | Version | License | Where | License text |
| --- | --- | --- | --- | --- |
| [SDL 3](https://libsdl.org) | 3.4.20 (fetched at build time, commit `f00dc919ca35b9756316e86c4777392de442f9b2`) | Zlib | linked into the desktop and web builds | `LICENSE.txt` in the SDL sources |
| [Lua](https://www.lua.org) | 5.4.9 | MIT | `third_party/lua` | [`third_party/lua/LICENSE`](third_party/lua/LICENSE) |
| [stb_image, stb_image_write, stb_vorbis](https://github.com/nothings/stb) | commit `9600522de539` | MIT or Unlicense (public domain) | `third_party/stb` | [`LICENSES/MIT.txt`](LICENSES/MIT.txt), [`LICENSES/Unlicense.txt`](LICENSES/Unlicense.txt) |
| [cgltf](https://github.com/jkuhlmann/cgltf) | 1.15 | MIT | `third_party/cgltf` | [`third_party/cgltf/LICENSE`](third_party/cgltf/LICENSE) |
| [Unity](https://github.com/ThrowTheSwitch/Unity) (tests only) | 2.6.1 | MIT | `third_party/unity` | [`third_party/unity/LICENSE`](third_party/unity/LICENSE) |
| [LuaCov](https://github.com/lunarmodules/luacov) (tests only) | 0.17.0 | MIT | `third_party/luacov` | [`third_party/luacov/LICENSE`](third_party/luacov/LICENSE) |
| [Emscripten](https://emscripten.org) runtime (web builds) | 4.0.23 | MIT / University of Illinois NCSA | generated JavaScript and libc in the web build | upstream `LICENSE` |

The components compiled into the game are also listed at runtime in the credits
(`engine.credits()`, from `assets/base/data/third_party.lua`). Exact revisions of the
vendored sources are recorded in [`third_party/VENDORED.md`](third_party/VENDORED.md).
