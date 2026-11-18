# test-automation Specification

## Purpose
Defines the automated quality gates every change must pass: unit, scenario, replay and web
tests, 95 % coverage for C and Lua, full spec-scenario coverage and cross-platform CI.

## Requirements

### Requirement: C unit tests
Every C module SHALL have unit tests runnable through CTest. A failing assertion SHALL make the test executable exit non-zero and report file, line and test name.

#### Scenario: Failing unit test
- **WHEN** a unit test assertion fails
- **THEN** `ctest` reports the test as failed with file, line and test name
- **AND** the CI job fails

### Requirement: Lua unit tests
Lua modules in packages SHALL be testable with test files run by the headless engine (`--test <path>`), using the engine's `test` module for assertions. Results SHALL be reported as JUnit XML.

#### Scenario: Run Lua tests
- **WHEN** `zauberer --headless --test tests/lua` runs
- **THEN** every test file is executed and a JUnit XML report is written

### Requirement: Scenario tests
The engine SHALL run scripted game scenarios with `--headless --scenario <file>`. A scenario may set the seed, inject input, step ticks, inspect state, compute state hashes and assert. The exit code SHALL be 0 only if every assertion passes.

#### Scenario: Passing scenario
- **WHEN** a scenario steps 600 ticks and asserts on a script-visible value that holds
- **THEN** the process exits with code 0 and reports the scenario as passed

#### Scenario: Failing scenario
- **WHEN** an assertion in a scenario fails
- **THEN** the process exits non-zero and reports the scenario name, tick and assertion message

### Requirement: Replay tests
A replay test SHALL combine a recorded input stream with expected state hashes at given ticks. A mismatch SHALL report the first tick whose hash differs.

#### Scenario: Divergence detected
- **WHEN** simulation behaviour changes so a replay's hash differs at tick 300
- **THEN** the replay test fails and reports tick 300 as the first divergence

### Requirement: Coverage gates
CI SHALL fail if combined line coverage from all test levels is below 95 % for C sources in `src/` or below 95 % for Lua sources in `assets/base/scripts`, or if either figure drops below the value recorded on the main branch.

#### Scenario: Coverage below threshold
- **WHEN** a change brings C line coverage to 94.8 %
- **THEN** the coverage job fails and lists the least-covered files

#### Scenario: Coverage ratchet
- **GIVEN** the main branch records 97.1 % Lua coverage
- **WHEN** a change lowers it to 96.5 %
- **THEN** the coverage job fails

### Requirement: Governed coverage exclusions
Coverage exclusions beyond `third_party/` and `tests/` SHALL be listed in a reviewed exclusion file with a written justification per entry. Excluded lines SHALL stay below 1 % of measured lines.

#### Scenario: Unjustified exclusion
- **WHEN** an exclusion entry has no justification text
- **THEN** the coverage job fails naming the entry

### Requirement: Spec scenario coverage
Every scenario in `openspec/specs/` and in active changes SHALL be referenced by at least one automated test using the tag `<capability>: <scenario name>`. CI SHALL fail and list unreferenced scenarios.

#### Scenario: Untested scenario
- **WHEN** a spec scenario has no test referencing it
- **THEN** the spec-coverage check fails and prints the capability and scenario name

### Requirement: Cross-platform CI
Every push and pull request SHALL run lint, Linux coverage, Linux sanitizer, Windows, macOS and web jobs. A determinism job SHALL fail if state or generation hashes differ between platforms.

#### Scenario: Platform-specific failure
- **WHEN** a change passes on Linux but fails a unit test on Windows
- **THEN** the overall CI status is failed

#### Scenario: Hash mismatch between platforms
- **WHEN** the web job reports a different replay hash than the Linux job
- **THEN** the determinism job fails and shows both hashes

### Requirement: Web smoke tests
CI SHALL load the web build in a headless browser. It SHALL check that frames render without console errors on desktop emulation, and that touch mode is active under mobile emulation.

#### Scenario: Mobile emulation
- **WHEN** the web build is opened in a mobile device emulation
- **THEN** the engine reports touch mode and renders frames without console errors

### Requirement: Leak detection
Test builds SHALL track allocations made through engine allocators, and a test SHALL fail if memory allocated during it remains unreleased at its end.

#### Scenario: Leaking test
- **WHEN** code under test allocates and does not free a block
- **THEN** the test fails and reports the allocation site
