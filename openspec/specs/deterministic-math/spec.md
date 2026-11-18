# deterministic-math Specification

## Purpose
Provides transcendental math functions that give bit-identical results on every platform
and compiler, so that world generation and other seeded computations reproduce exactly.

## Requirements

### Requirement: Platform-independent results
The engine SHALL provide sine, cosine, tangent, arcus tangent (one and two argument), arcus sine, arcus cosine, exponential, natural logarithm and power functions whose results are bit-identical on all supported platforms.

#### Scenario: Golden values
- **WHEN** each function is evaluated on a fixed set of 100,000 arguments per function
- **THEN** the hash of all results equals the golden value recorded in the tests, on the Linux, Windows and WebAssembly builds

### Requirement: Accuracy
The deterministic functions SHALL stay within 2 units in the last place of the correctly rounded result for sine, cosine, arcus tangent, exponential and logarithm over the argument ranges used by world generation, and within a relative error of 1e-13 for power.

#### Scenario: Compared with a reference
- **WHEN** the functions are compared with the platform math library over their ranges
- **THEN** no result differs by more than the stated tolerance

### Requirement: Special values
The deterministic functions SHALL handle zero, infinity, NaN and out-of-domain arguments like the C standard library does.

#### Scenario: Out of domain
- **WHEN** the logarithm of a negative number or the arcus sine of 2 is computed
- **THEN** the result is NaN
