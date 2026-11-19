# release-pipeline Specification

## Purpose
Turns a commit into release artefacts: one version source, store-agnostic archives per
desktop platform with the shipped layout, a web archive, optional signing, checksums, the
web deploy to GitHub Pages and a manual release workflow.

## Requirements

### Requirement: One release version source
The release version SHALL be defined in exactly one place, `ZB_VERSION` in `src/core/version.h`, as a semantic version with an optional pre-release suffix. The build SHALL derive the project and package versions from it, and `assets/base/package.lua` SHALL carry the same version.

#### Scenario: One place to change the version
- **WHEN** the repository's version sources are read
- **THEN** `ZB_VERSION` is a semantic version, `assets/base/package.lua` and the changelog name the same version, and `CMakeLists.txt` reads the header instead of repeating the version

### Requirement: Release readiness check
`tools/release.py check` SHALL fail (exit code 1), naming each problem, when the version is not a semantic version, when the base package's version differs from it, when `CHANGELOG.md` has no level-2 heading for the version, or when the git tree has uncommitted changes; `--allow-dirty` SHALL skip only the last check.

#### Scenario: A consistent release passes the check
- **WHEN** `release.py check` runs in a clean repository whose header, base package and changelog agree on `1.0.0-rc.1`
- **THEN** it exits with 0 and reports that `1.0.0-rc.1` is ready to release

#### Scenario: Version mismatch is refused
- **WHEN** `assets/base/package.lua` says `0.1.0` while `ZB_VERSION` is `1.0.0-rc.1`
- **THEN** the check exits with 1 and names both versions and both files

#### Scenario: Missing changelog entry is refused
- **WHEN** `CHANGELOG.md` mentions the version only in running text but has no heading for it
- **THEN** the check exits with 1 and says the changelog has no entry for the version

#### Scenario: A dirty tree is refused
- **WHEN** the repository has an untracked file
- **THEN** the check exits with 1 and names the file
- **AND** with `--allow-dirty` the same check passes and says the clean-tree check was skipped

### Requirement: Store-agnostic desktop packages
CPack SHALL archive the shipped layout (concept §9.2) as one folder named `zauberer-<version>-<os>-<arch>`, a `.tar.gz` on Linux and a `.zip` on Windows and macOS, holding the `zauberer` and `zauberer-relay` executables, `assets/base`, `mods/` (the repository's mods if any, and a README), `LICENSE`, `LICENSES/`, the third-party license texts and notices, a generated `CREDITS.md`, `README.md` and `CHANGELOG.md`. The game SHALL find `assets/` and `mods/` next to its executable.

#### Scenario: Package has the shipped layout
- **WHEN** a Linux `.tar.gz` and a Windows `.zip` with the complete layout are verified
- **THEN** no problem is reported, and the install rules name the executables, assets, mods, licenses, notices, credits and changelog

### Requirement: Package command
`tools/release.py package` SHALL configure and build a preset (default `release`), run CPack, verify the archive's layout and copy it into the dist folder (default `build/dist`). It SHALL fail (exit code 2) when a step fails or the layout is incomplete. With `--dry-run` it SHALL only print the commands and the archive name.

#### Scenario: Package plan without building
- **WHEN** `release.py package --dry-run` runs for the platforms `linux-x86_64`, `windows-x86_64` and `macos-arm64`
- **THEN** it prints the configure, build and CPack commands and the archives `zauberer-1.0.0-rc.1-linux-x86_64.tar.gz`, `zauberer-1.0.0-rc.1-windows-x86_64.zip` and `zauberer-1.0.0-rc.1-macos-arm64.zip`
- **AND** it runs no command and writes no file

#### Scenario: Incomplete package is refused
- **WHEN** an archive lacks `assets/base` and `CREDITS.md`, has a non-executable `zauberer`, or holds a top-level folder named unlike the archive
- **THEN** the verification lists each of these problems

### Requirement: Web package
`tools/release.py web` SHALL assemble the single-threaded web build (`build/web-st/bin`) together with the threaded engine files of `build/web/bin`, or whichever of the two exists, into the folder `zauberer-<version>-web` and zip it reproducibly (sorted entries, fixed timestamps) as `zauberer-<version>-web.zip`. It SHALL fail (exit code 2) when no web build exists or a given folder has no `index.html`.

#### Scenario: Web archive from the web builds
- **WHEN** both web builds exist and `release.py web` runs twice
- **THEN** `zauberer-1.0.0-rc.1-web.zip` holds one folder with the page, the loader, the assets and both engines, and the two zips are byte-identical
- **AND** with only the single-threaded build the zip has no threaded engine, and a dry run writes nothing

#### Scenario: Missing web build is refused
- **WHEN** no web build exists, or the given folder has no `index.html`
- **THEN** `release.py web` exits with 2 and says why

### Requirement: Checksums
`tools/release.py sums` SHALL write `SHA256SUMS` in the dist folder with one `<sha256>  <name>` line per artefact (every file directly in the folder except `SHA256SUMS` and signatures), sorted by name.

#### Scenario: Checksums for every artefact
- **WHEN** the dist folder holds three archives, an older signature and an assembled web folder, and `release.py sums` runs twice
- **THEN** `SHA256SUMS` lists exactly the three archives with their SHA-256 digests, sorted by name

### Requirement: Optional signing
Signing SHALL be optional: with `ZB_SIGN_WINDOWS_CERT` the Windows executables SHALL be signed with `signtool`, with `ZB_SIGN_MACOS_IDENTITY` the macOS executables SHALL be signed and verified with `codesign`, both before CPack archives them, and with `ZB_SIGN_GPG_KEY` `SHA256SUMS` SHALL get a detached GPG signature. Without a variable the step SHALL log that signing is skipped; a failing signing command SHALL fail the step. Passwords SHALL not be logged.

#### Scenario: Signing is skipped without keys
- **WHEN** executables are signed for Windows, macOS and Linux and the checksums are written without any signing variable
- **THEN** each step logs that signing is skipped or not applicable, succeeds and runs no signing command

#### Scenario: Signing with configured keys
- **WHEN** the Windows certificate, the macOS identity and the GPG key are set
- **THEN** `signtool` signs both Windows executables, `codesign` signs and verifies each macOS executable, and `gpg` writes `SHA256SUMS.asc`, without logging the certificate password
- **AND** a failing signing command fails the step

### Requirement: Web deploy to GitHub Pages
`tools/deploy_web.py` SHALL publish a web build folder (one with an `index.html`) to the `gh-pages` branch through a temporary detached `git worktree` that is always removed, creating no local branch. The published tree SHALL be the build plus `.nojekyll` and a `version.txt` with the version and source commit, which the commit message SHALL name. When the branch already holds the build, nothing SHALL be committed. `--dry-run` SHALL print the changes without committing or pushing.

#### Scenario: Deploy publishes the web build
- **WHEN** a build is deployed to a local bare repository, and then a second build with fewer files
- **THEN** `gh-pages` holds the build, `.nojekyll` and `version.txt`, the commit message names the version and source commit, the second commit replaces the first build on top of it, and deploying the same build again changes nothing
- **AND** the source repository gains no branch and keeps no extra worktree

#### Scenario: Web deploy dry run changes nothing
- **WHEN** `--dry-run` runs against a remote without `gh-pages` and against one with an older build
- **THEN** it lists the added, modified and deleted files and the commit it would make, and the remote is unchanged

### Requirement: Deploy push retries
The deploy's push SHALL be retried up to 4 times with exponential backoff (2, 4, 8 and 16 s); when the last retry fails, the tool SHALL fail (exit code 2).

#### Scenario: Push is retried with backoff
- **WHEN** the push fails
- **THEN** it is retried after 2, 4, 8 and 16 seconds, a later success ends the retries, and after five failed attempts the tool exits with 2

### Requirement: Manual release workflow
`.github/workflows/release.yml` SHALL start only by hand (`workflow_dispatch`), need no secrets and read-only repository permissions, build the Linux, Windows and web packages with `tools/release.py`, and upload them together with `SHA256SUMS` as workflow artifacts. It SHALL not deploy, tag or publish a release. Every workflow of the repository SHALL start only by hand.

#### Scenario: Release workflow runs only by hand
- **WHEN** the workflow files are read
- **THEN** every workflow's only trigger is `workflow_dispatch`, and the release workflow references no secrets, builds Linux, Windows and web packages with `release.py`, uploads them, and writes the checksums
