# weather Specification

## Purpose
Defines the world's weather as a function of place and time (drifting systems,
climate-driven cloud, rain and snow, thunderstorms), how the active tier uses it,
lightning, and how the sky and the air look under it.

## Requirements

### Requirement: World weather
The world's weather SHALL be a pure function of the world, the place and the game time: systems drifting with the prevailing winds of their latitude, cloud and rain as often as the place's climate brings them, snow below freezing, and thunderstorms in warm heavy cloud that strengthen the wind.

#### Scenario: Prevailing winds by latitude
- **WHEN** the prevailing wind is asked for at the equator, 15 degrees north and south, and 50 degrees north
- **THEN** it is easterly at the equator, easterly and towards the equator in the tropics, and westerly at 50 degrees

#### Scenario: Climate decides the weather
- **WHEN** the weather of the driest and the wettest of a row of warm places is sampled hourly for two months
- **THEN** the wet place is cloudier and rainier, and every value is in range

#### Scenario: Weather systems drift with the wind
- **WHEN** the clouds at 50 degrees north are compared six hours later at places downwind and upwind by the westerlies' drift
- **THEN** they match better downwind

#### Scenario: Snow, rain and thunderstorms
- **WHEN** far northern places are sampled through winter and tropical places through a month
- **THEN** it snows in the north and never rains below freezing, some tropical hours are stormy, storms blow harder than clear weather, and the same place and time always give the same weather

### Requirement: Weather in the game
The engine SHALL apply the world's weather at the player to fire, surface water and the air once a second unless scripts set a weather; storms SHALL send lightning that ignites dry fuel and flashes the scene; clouds SHALL grey the sky and rain and snow SHALL be drawn around the camera.

#### Scenario: Clouds darken the sky
- **WHEN** the noon daylight is overcast with no cloud and with full cloud
- **THEN** no cloud changes nothing, and full cloud hides the sun's disc, dims the light below 40 % and greys the blue sky

#### Scenario: Weather in the game
- **WHEN** a scenario reads the weather at the carpet, sets one, returns to the world's, strikes lightning, sets a storm, and looks at an overcast sky and rain
- **THEN** the world's weather is in use at first, the scripts' holds until released, lightning sets the fuel alight and is counted, a storm brings more strikes, the overcast sky is not blue, rain is drawn, and invalid weather is refused
