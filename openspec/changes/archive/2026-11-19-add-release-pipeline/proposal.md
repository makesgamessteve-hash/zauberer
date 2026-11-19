# Proposal: Release Pipeline

## Why

Milestone **M11 (Release)** ends with 1.0 release candidates on all platforms
(`docs/implementation-plan.md` §5). Until now a "release" was a build directory: the
version was written in three places (`CMakeLists.txt`, `src/core/version.h`,
`assets/base/package.lua`), nothing assembled the shipped layout of concept §9.2 into an
archive, there was no changelog, no checksums, no signing story and no repeatable way to
publish the web build. This change adds a store-agnostic release pipeline and makes the
first release candidate, `1.0.0-rc.1`.

## What Changes

- **One version source:** `ZB_VERSION` in `src/core/version.h` (now `1.0.0-rc.1`).
  `CMakeLists.txt` reads it for `project(VERSION)` and the package names;
  `assets/base/package.lua` must carry the same version, checked by `tools/release.py
  check`. The scripting API version (1.0) is unchanged.
- **Store-agnostic packages:** `cmake/ZbPackaging.cmake` adds install rules for the shipped
  layout (the `zauberer` executable, `zauberer-relay`, `assets/base`, `mods/` with a
  README or the repository's mods, `LICENSE`, `LICENSES/`, the third-party license texts,
  `THIRD_PARTY_NOTICES.md`, a generated `CREDITS.md`, `README.md` and `CHANGELOG.md`) and
  CPack archives named `zauberer-<version>-<os>-<arch>`: `.tar.gz` on Linux, `.zip` on
  Windows and macOS, each one plain folder. A CPack package preset `release` joins the
  existing `release` configure and build presets.
- **Signing where applicable:** optional. A CPack pre-build script signs the staged
  executables with `signtool` (`ZB_SIGN_WINDOWS_CERT`) or `codesign`
  (`ZB_SIGN_MACOS_IDENTITY`), and `release.py sums` signs `SHA256SUMS` with GPG
  (`ZB_SIGN_GPG_KEY`). Without the variables, each step logs that signing is skipped.
- **`tools/release.py`:** `version`, `check` (version consistency, a changelog entry, a
  clean tree; `--allow-dirty` skips the last), `package` (configure, build, CPack, verify
  the layout, copy to `build/dist`; `--dry-run` prints the plan), `web` (zips the
  `web-st` and/or `web` builds as `zauberer-<version>-web.zip`), `sums` (`SHA256SUMS`),
  `sign-binaries` and `credits`.
- **`tools/deploy_web.py`:** publishes a built web directory to the `gh-pages` branch
  through a temporary detached `git worktree`, adds `.nojekyll` and `version.txt`, commits
  with the version and source commit, and pushes with up to 4 retries and exponential
  backoff; `--dry-run` prints the changes without committing or pushing.
- **Docs and workflow:** `docs/release.md` (how to cut a release), `CHANGELOG.md` with the
  `1.0.0-rc.1` entry for M0–M11, README links, and `.github/workflows/release.yml`, a
  manual (`workflow_dispatch`) workflow that builds the Linux, Windows and web packages
  and uploads them with `SHA256SUMS` as artifacts, without secrets.
- Platform differences: Linux ships a `.tar.gz` (executables covered by the GPG signature
  of the checksums), Windows a `.zip` with Authenticode signing when configured, macOS a
  `.zip` folder with codesign when configured (no `.app` bundle or notarisation yet), and
  the web a zip of the single-threaded build plus the threaded engine, deployed to GitHub
  Pages. Mobile web and WebXR use the web build; nothing differs for them.

## Capabilities

### New Capabilities

- `release-pipeline`: the release version source, the desktop and web packages, optional
  signing, checksums, the web deploy and the manual release workflow.

### Modified Capabilities

None.

## Impact

- **New files:** `cmake/ZbPackaging.cmake`, `cmake/ZbSign.cmake`,
  `packaging/mods-README.md`, `tools/release.py`, `tools/deploy_web.py`,
  `tests/tools/test_release.py`, `tests/tools/test_deploy_web.py`, `docs/release.md`,
  `CHANGELOG.md`, `.github/workflows/release.yml`.
- **Changed files:** `CMakeLists.txt` (version from the header, packaging),
  `CMakePresets.json` (package preset), `src/core/version.h` and
  `assets/base/package.lua` (`1.0.0-rc.1`), `tests/unit/script/test_script.c` (compares
  against `ZB_VERSION` instead of a literal), the generated Lua API reference, `README.md`.
- **Engine behaviour:** none beyond the version string. Replays and co-op compare the
  package set (ids and versions) between peers built from the same release, so they stay
  compatible; peers on 0.1.0 and 1.0.0-rc.1 no longer match, as intended.
- **Co-op:** none. The relay ships in every desktop package.
- **CI:** workflows stay manual-only. The tool tests run in the existing `Tool tests` step
  and the `python_tools` CTest.
