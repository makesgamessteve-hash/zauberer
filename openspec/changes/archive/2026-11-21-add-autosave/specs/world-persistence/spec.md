## ADDED Requirements

### Requirement: Autosave
The game SHALL save on its own every five minutes of play, at each turn of the story, before sleep and when the web page is hidden; not while the heir lies defeated (then once they are up again), and in company only on the host; a line on the HUD SHALL say the game was saved for two seconds.

#### Scenario: Progress survives a lost game
- **WHEN** the story turns, the heir is defeated and gets up, sleeps on foot, a client tries to save, and the game ends without quitting and starts again
- **THEN** each turn, the getting up and the sleep save the game (not while defeated, not the client), the HUD says so for two seconds, and the next start continues the autosaved game
