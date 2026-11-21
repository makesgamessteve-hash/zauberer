## MODIFIED Requirements

### Requirement: Escape opens the menu in play
In play Escape SHALL open the menu; while it is open the heir's own actions SHALL be idle; alone the world SHALL stand still, in a co-op session it goes on; Escape SHALL close it.

#### Scenario: Escape opens the menu in play
- **WHEN** Escape is pressed in flight alone, a cast is tried and Escape pressed again; and the menu is opened in a co-op session
- **THEN** the menu opens, no cast happens, no game time passes alone, the menu closes and time goes on; in company time passes with the menu open

## ADDED Requirements

### Requirement: Losing focus
When the window loses focus, is hidden or minimised, the app goes to the background or the web page is hidden, the game SHALL open the menu in play and mute its sound, and on the web save the game; with focus back the sound SHALL return and the menu stay open.

#### Scenario: Focus lost and back
- **WHEN** focus is lost in flight and comes back, and the web page is hidden and shown
- **THEN** the menu opens, the sound is muted and the world stands still; with focus back the sound returns and the menu is still open; the page's visibility reaches the game
