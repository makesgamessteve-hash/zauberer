## ADDED Requirements

### Requirement: Mouse capture in the browser
The web page SHALL capture the mouse (pointer lock) when the game is clicked with a mouse, so the view turns with the mouse's movement without the cursor leaving the page; the click that captures SHALL NOT reach the game, later clicks SHALL. While the mouse is free, a line SHALL say how to capture it; Escape releases it. Touch and pen input SHALL NOT be captured.

#### Scenario: The mouse is captured in the browser
- **WHEN** the running game is clicked with a mouse, clicked again, and the capture released
- **THEN** the mouse is captured and the hint hidden, only the second click reaches the game, and the hint shows again
