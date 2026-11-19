# Tasks

## 1. Version

- [x] 1.1 `ZB_VERSION` in `src/core/version.h` becomes `1.0.0-rc.1` and the one version source: `CMakeLists.txt` reads it for `project(VERSION)`, `assets/base/package.lua` matches, the script unit test compares against `ZB_VERSION`; regenerate the Lua API reference
- [x] 1.2 `tools/release.py version` and `check` (semantic version, base package, changelog entry, clean tree with `--allow-dirty`); verify "One place to change the version", "A consistent release passes the check", "Version mismatch is refused", "Missing changelog entry is refused", "A dirty tree is refused"

## 2. Desktop packages

- [x] 2.1 `cmake/ZbPackaging.cmake`: install rules for the shipped layout (executables, assets, `mods/` with `packaging/mods-README.md`, licenses, third-party license texts, notices, generated `CREDITS.md`, readme, changelog) and CPack (`.tar.gz` on Linux, `.zip` on Windows and macOS, `zauberer-<version>-<os>-<arch>`); a `release` package preset
- [x] 2.2 `release.py package` (configure, build, CPack, layout verification, copy to `build/dist`, `--dry-run`) and `credits`; verify "Package plan without building", "Package has the shipped layout", "Incomplete package is refused"
- [x] 2.3 Build and package on Linux for real; extract the archive and run the demo scenario and `zauberer-relay --help` from the extracted folder

## 3. Web package, checksums and signing

- [x] 3.1 `release.py web` (assemble and reproducible zip); verify "Web archive from the web builds", "Missing web build is refused"
- [x] 3.2 `release.py sums`, `sign-binaries` (signtool, codesign) through the CPack pre-build script `cmake/ZbSign.cmake`, and GPG signing of `SHA256SUMS`; verify "Checksums for every artefact", "Signing is skipped without keys", "Signing with configured keys"

## 4. Web deploy

- [x] 4.1 `tools/deploy_web.py` (temporary detached worktree, `.nojekyll`, `version.txt`, commit message with version and commit, push retries with backoff, `--dry-run`, `--no-push`); verify "Deploy publishes the web build", "Web deploy dry run changes nothing", "Push is retried with backoff" against a temporary bare repository

## 5. Docs and workflow

- [x] 5.1 `docs/release.md`, `CHANGELOG.md` with the `1.0.0-rc.1` entry (M0–M11), links in the README
- [x] 5.2 `.github/workflows/release.yml` (`workflow_dispatch` only, no secrets, Linux, Windows and web packages as artifacts with `SHA256SUMS`); verify "Release workflow runs only by hand"
- [x] 5.3 Map every scenario to a tagged test in `tests/tools/test_release.py` or `tests/tools/test_deploy_web.py` (`tools/spec_coverage.py`); tool tests, license, REUSE, credits and luacheck checks pass
