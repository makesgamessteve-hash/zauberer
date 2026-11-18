## MODIFIED Requirements

### Requirement: Required materials
Building the materials SHALL fail with a message when air, water or any terrain surface material (soil, sand, rock, gravel, snow, ice, mud, ash) has no usable entry, and SHALL keep at most 250 materials.

#### Scenario: The engine's materials are required
- **WHEN** any one of the required materials is missing, or more than 250 are defined
- **THEN** building fails naming the missing material, or the extra entries are left out
