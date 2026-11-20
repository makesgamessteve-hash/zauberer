## ADDED Requirements

### Requirement: Assets read while the engine starts are preloaded in the browser
Files the engine reads while it starts (not on demand), such as the terrain atlas and its texture, SHALL be in the web build's boot set, so they are there when they are read.

#### Scenario: Assets read at start are there in the browser
- **WHEN** the game starts in the browser
- **THEN** no file read at start is missing or empty (the terrain atlas loads)
