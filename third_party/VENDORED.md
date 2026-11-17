# Vendored third-party code

These sources are copied unmodified from their upstream repositories at the
pinned revisions below. Each directory contains the upstream license. SDL3 is
not vendored: CMake fetches it at the pinned tag and verifies the commit
(`cmake/ZbDependencies.cmake`).

| Directory | Project | Version | Commit | License |
| --- | --- | --- | --- | --- |
| `lua/` | Lua (https://github.com/lua/lua) | 5.4.9 | `312b9efaa1061c2c4cad08554dbc1351c3270eef` | MIT |
| `stb/` | stb_image, stb_image_write, stb_vorbis (https://github.com/nothings/stb) | master | `9600522de53973336fa033a289858a4354cbb04c` | MIT or public domain |
| `cgltf/` | cgltf (https://github.com/jkuhlmann/cgltf) | 1.15 | `360db1a95480fe102ae9c69b27c5d101167ff5ba` | MIT |
| `unity/` | Unity test framework (https://github.com/ThrowTheSwitch/Unity) | 2.6.1 | `cbcd08fa7de711053a3deec6339ee89cad5d2697` | MIT |
| `luacov/` | LuaCov (https://github.com/lunarmodules/luacov) | 0.17.0 | `b1f9eae400da976b93edb7f94cf5d05f538a0655` | MIT |
| `impl/` | Project-authored implementation units for the single-header libraries | — | — | MIT |

Lua sources exclude the upstream test suite (`testes/`, `ltests.c`, `ltests.h`),
`onelua.c`, the manual and the makefile.
