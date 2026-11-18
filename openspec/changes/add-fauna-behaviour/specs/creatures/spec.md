## MODIFIED Requirements

### Requirement: Creature budget
Two hundred creatures in a desktop active tier, wolves hunting deer among them, all thirsty and leaving scent, SHALL cost at most 0.5 ms a tick on average in a release build on Linux.

#### Scenario: Creature budget
- **WHEN** 200 wolves, deer and birds live around a mage on foot for five seconds after a noise, the wolves hunting the deer
- **THEN** most are in the active tier and a tick costs at most 0.5 ms on average
