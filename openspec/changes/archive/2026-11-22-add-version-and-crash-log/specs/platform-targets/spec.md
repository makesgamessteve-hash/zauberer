## ADDED Requirements

### Requirement: Version and crash log
The version SHALL be shown on the title and in the menu; a player's game SHALL keep its last 200 lines logged and on a fatal error (a failed assertion, a fatal signal) write them with the error to crash.log in the user folder and say where; in the browser a stopped engine SHALL say so on the page with a reload button.

#### Scenario: A fatal error
- **WHEN** 250 lines are logged and the crash log written, and a player's game fails an assertion on purpose
- **THEN** the log holds the reason and only the last 200 lines, the game stops with a non-zero code, crash.log holds the assertion and the lines logged before it, and the output says where it is

#### Scenario: A fatal error in the browser
- **WHEN** the engine stops while the page plays
- **THEN** the page says the game stopped and why, and offers a reload button
