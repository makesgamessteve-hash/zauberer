## MODIFIED Requirements

### Requirement: Weather in the game
The engine SHALL apply the world's weather at the player to fire, surface water and the air once a second unless scripts set a weather (wind, rain, snow, cloud, storm and the air's temperature, each kept when left out); storms SHALL send lightning that ignites dry fuel and flashes the scene; clouds SHALL grey the sky and rain and snow SHALL be drawn around the camera.

#### Scenario: Clouds darken the sky
- **WHEN** the noon daylight is overcast with no cloud and with full cloud
- **THEN** no cloud changes nothing, and full cloud hides the sun's disc, dims the light below 40 % and greys the blue sky

#### Scenario: Weather in the game
- **WHEN** a scenario reads the weather at the carpet, sets one, sets the air's temperature, returns to the world's, strikes lightning, sets a storm, and looks at an overcast sky and rain
- **THEN** the world's weather is in use at first, the scripts' holds until released, the temperature set is the one in use and kept when left out, lightning sets the fuel alight and is counted, a storm brings more strikes, the overcast sky is not blue, rain is drawn, and invalid weather is refused
