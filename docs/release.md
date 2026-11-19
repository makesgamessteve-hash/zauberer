<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# Cutting a release

This is how a Zauberer release (a release candidate or a final version) is made: the
version, the changelog, the gates, the packages for each platform, signing, checksums,
the web deploy and the tag. The tools are `tools/release.py` and `tools/deploy_web.py`
(standard-library Python 3); the OpenSpec capability is `release-pipeline`.

| Artefact | Made on | Name |
| --- | --- | --- |
| Linux archive | Linux | `zauberer-<version>-linux-x86_64.tar.gz` |
| Windows archive | Windows (MSVC) | `zauberer-<version>-windows-x86_64.zip` |
| macOS archive | macOS | `zauberer-<version>-macos-arm64.zip` (or `-x86_64`, `-universal`) |
| Web archive | any, with the Emscripten SDK | `zauberer-<version>-web.zip` |
| Checksums | anywhere, once all archives are together | `SHA256SUMS` (and `SHA256SUMS.asc`) |

Every archive holds one plain folder named like the archive. No installer and no store
metadata: the same folder can be uploaded to itch.io, a store's depot or a web server.

```
zauberer-1.0.0-rc.1-linux-x86_64/
  zauberer  zauberer-relay        the game and the co-op relay (.exe on Windows)
  assets/base/                    the base game package (concept §9.2)
  mods/                           README.md, and the repository's mods when there are any
  LICENSE  LICENSES/              MIT for code, CC BY 4.0 for art and text, Unlicense (stb)
  third_party/                    license texts of SDL 3, Lua and cgltf
  THIRD_PARTY_NOTICES.md  CREDITS.md  README.md  CHANGELOG.md
```

The game finds `assets/` and `mods/` next to its executable, so the folder runs from
wherever it is extracted. Player data (settings, saves, per-user mods) stays in the user
data folder (`%APPDATA%/Zauberer`, `$XDG_DATA_HOME/zauberer`, `~/Library/Application
Support/Zauberer`), so an update replaces the folder without touching saves.

## 1. Bump the version

The version has one source: `ZB_VERSION` in [`src/core/version.h`](../src/core/version.h),
a [semantic version](https://semver.org/) with an optional pre-release suffix
(`1.0.0-rc.1`, `1.0.0-rc.2`, `1.0.0`). CMake reads it for `project(VERSION)` and the package
names. Change it there, then:

- set the same `version` in [`assets/base/package.lua`](../assets/base/package.lua) (the base
  package's version is part of the package-set hash that co-op peers compare, so players
  need the same release to play together),
- regenerate the Lua API reference if a doc comment changed: `python3 tools/gen_api_docs.py`.

The scripting API version (`ZB_API_VERSION`) changes only when the Lua API changes
incompatibly (major) or grows (minor); it is independent of the release version.

## 2. Update the changelog

Add a section to [`CHANGELOG.md`](../CHANGELOG.md) with a heading
`## [<version>] - <YYYY-MM-DD>` that summarises what changed for players and modders.
Commit the version bump and the changelog together, then check:

```sh
python3 tools/release.py check      # version consistency, changelog entry, clean tree
```

`check` fails when `assets/base/package.lua` disagrees with `ZB_VERSION`, when the
changelog has no heading for the version, or when the tree has uncommitted changes
(`--allow-dirty` skips that last check while preparing).

## 3. Run the full gates

CI is started by hand to save build minutes, so the full gates run locally before a
release, on the commit being released (the same gates as at the end of each milestone):

```sh
# Coverage gate (C and Lua, >= 95 % with the ratchet), as in .github/workflows/ci.yml
cmake --preset coverage && cmake --build --preset coverage && ctest --preset coverage
#   then gcovr + tools/coverage_gate.py c and tools/luacov_report.py + coverage_gate.py lua
# Sanitizers
cmake --preset asan && cmake --build --preset asan && ctest --preset asan -LE render
# Web: headless engine and unit tests under Node.js, then the browser tests (Playwright)
source "$EMSDK/emsdk_env.sh"
for p in web web-st web-node; do cmake --preset $p && cmake --build --preset $p; done
ctest --preset web-node
cd tests/web && npm install && node --test --test-concurrency=1 web.test.mjs
```

Also run the lint steps of the CI `lint` job (clang-format, `reuse lint`, OpenSpec
validation, tool tests, `check_includes.py`, `check_licenses.py`, `check_credits.py`,
`spec_coverage.py`, `gen_api_docs.py --check`, luacheck, reproducible generated assets).
Starting the CI workflow by hand (Actions > CI > Run workflow) adds Windows and the
cross-platform determinism comparison.

## 4. Build the packages

On each desktop platform, from a clean checkout of the release commit:

```sh
python3 tools/release.py package --dry-run    # prints the plan: commands and archive name
python3 tools/release.py package              # configure + build the release preset, CPack,
                                              # verify the layout, copy to build/dist/
```

`package` runs `cmake --preset release`, `cmake --build --preset release` and CPack
(`cpack --preset release` does the same CPack step by hand). It then checks the archive
against the shipped layout and copies it to `build/dist/`. Options: `--preset`,
`--dist <dir>`, `--platform <os-arch>` (the name's platform part), and
`--configure-arg <arg>` for extra configure arguments (for example
`-DFETCHCONTENT_SOURCE_DIR_SDL3=<dir>` to build with an SDL checkout that is already there).
The release preset also builds the tests: run `ctest --preset release` before packaging
if you like.

- **Linux:** a `.tar.gz`. Build on the oldest distribution you want to support (glibc is
  the only shared dependency besides the system's graphics and audio libraries, which SDL
  loads at run time). An AppImage is not made.
- **Windows:** a `.zip`, built in a Developer Command Prompt (MSVC) so that `cmake`
  finds the compiler.
- **macOS:** a `.zip` holding a plain folder (no `.app` bundle yet). For a universal
  binary configure with `--configure-arg=-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64`.

The web build:

```sh
source "$EMSDK/emsdk_env.sh"
for p in web web-st; do cmake --preset $p && cmake --build --preset $p; done
python3 tools/release.py web     # build/dist/zauberer-<version>-web.zip and the folder beside it
```

The web archive holds the single-threaded build (`zauberer-st.*`, the page, the loader
and the assets) and the threaded engine (`zauberer.js`, `zauberer.wasm`); the page picks
the threaded engine when the server sends the cross-origin isolation headers. `--web-st`
and `--web` choose other build folders; either one alone is enough.

The manual workflow [`.github/workflows/release.yml`](../.github/workflows/release.yml)
(Actions > Release > Run workflow) builds the Linux, Windows and web archives on GitHub's
runners and uploads them with `SHA256SUMS` as one artifact. It uses no secrets, so its
archives are unsigned; it never deploys or tags.

## 5. Sign (where applicable)

Signing is optional and happens only when the keys are configured through environment
variables; otherwise each step logs `signing skipped` and the release is unsigned.

| Variable | Platform | What happens |
| --- | --- | --- |
| `ZB_SIGN_WINDOWS_CERT` | Windows | `signtool sign /fd SHA256 /f <cert> /tr <timestamp> /td SHA256` signs `zauberer.exe` and `zauberer-relay.exe` before they are zipped. Optional: `ZB_SIGN_WINDOWS_PASSWORD`, `ZB_SIGN_TIMESTAMP_URL`, `ZB_SIGNTOOL`. |
| `ZB_SIGN_MACOS_IDENTITY` | macOS | `codesign --force --timestamp --options runtime --sign <identity>` signs (and verifies) both executables before they are zipped. Optional: `ZB_CODESIGN`. Notarisation is not automated yet: submit the zip with `xcrun notarytool submit --wait` if you need it. |
| `ZB_SIGN_GPG_KEY` | any | `gpg --armor --detach-sign` signs `SHA256SUMS` as `SHA256SUMS.asc` (step 6); this covers every archive, including Linux and the web. Optional: `ZB_GPG`. |

The executables are signed by CPack's pre-build script (`cmake/ZbSign.cmake`, which runs
`tools/release.py sign-binaries` on the staged folder), so set the variables in the shell
that runs `release.py package`. Keep the keys on the release machine; never commit them
or put them in a workflow.

## 6. Checksums

Collect every archive in one folder (default `build/dist/`) and run:

```sh
python3 tools/release.py sums            # writes build/dist/SHA256SUMS (+ .asc with ZB_SIGN_GPG_KEY)
cd build/dist && sha256sum -c SHA256SUMS # check them (gpg --verify SHA256SUMS.asc for the signature)
```

## 7. Deploy the web build

[`tools/deploy_web.py`](../tools/deploy_web.py) publishes a built web folder to the
`gh-pages` branch, which GitHub Pages serves (the **PLAY** link in the README):

```sh
python3 tools/deploy_web.py build/dist/zauberer-<version>-web --dry-run   # what would change
python3 tools/deploy_web.py build/dist/zauberer-<version>-web             # commit and push
```

It fetches `gh-pages` from `origin` (or starts the branch if there is none), checks it out
in a temporary detached `git worktree` (no local branch is created), replaces the files
with the build, adds `.nojekyll` and `version.txt` (the version and the source commit),
commits with the message `Deploy web build <version> (commit <hash>)` and pushes, retrying
up to 4 times with exponential backoff (2, 4, 8, 16 s). If the branch already holds the
build, nothing is committed. Options: `--remote`, `--branch`, `--version`, `--commit`,
`--retries`, `--no-push` (commit in the temporary worktree only) and `--repo`.

GitHub Pages does not send the cross-origin isolation headers, so the deployed page runs
the single-threaded engine; behind a server that sends them, the same folder runs threaded.

## 8. Tag and publish

```sh
git tag -a v<version> -m "Zauberer <version>"
git push origin v<version>
```

Then create the release on the hosting site (or a store page) from the tag, attach the
archives, `SHA256SUMS` and `SHA256SUMS.asc`, and paste the changelog section. After a
final release, start the next version's changelog section when the first change lands.
