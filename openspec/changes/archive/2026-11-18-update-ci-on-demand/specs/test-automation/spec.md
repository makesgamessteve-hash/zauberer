## MODIFIED Requirements

### Requirement: Cross-platform CI
CI SHALL run, when started (for now by hand, to save build minutes), lint, Linux coverage, Linux sanitizer, Windows, macOS and web jobs, and the full gates SHALL run locally at least once per milestone. A determinism job SHALL fail if state or generation hashes differ between platforms.

#### Scenario: Platform-specific failure
- **WHEN** a change passes on Linux but fails a unit test on Windows
- **THEN** the overall CI status is failed

#### Scenario: Hash mismatch between platforms
- **WHEN** the web job reports a different replay hash than the Linux job
- **THEN** the determinism job fails and shows both hashes
