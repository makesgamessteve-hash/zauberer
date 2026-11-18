# Proposal: CI on Demand

## Why

The project owner wants to save build minutes for now. CI ran every job on every push
and pull request, and the world corpus ran every night. That is many runner-hours a day
while changes land in quick succession.

## What Changes

- CI and the nightly world corpus are started by hand (`workflow_dispatch`); neither
  runs on push, pull request or a schedule.
- The full gates (coverage, sanitizers, web and browser tests) run locally once per
  milestone. Each change still runs its build, unit and engine tests, lint and spec
  checks before it lands.
- The requirement keeps what a CI run contains, so starting it by hand still checks
  every platform and the cross-platform determinism.
- Each workflow file says how to turn its trigger back on.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `test-automation`: CI runs on demand instead of on every push and pull request.

## Impact

- `.github/workflows/ci.yml`, `.github/workflows/nightly.yml`, `README.md` and the
  workflow test.
- Risk: platform-specific breakage (Windows, the web) is found only when CI is started
  or at the milestone gates. The local gates cover Linux, sanitizers and the web.
