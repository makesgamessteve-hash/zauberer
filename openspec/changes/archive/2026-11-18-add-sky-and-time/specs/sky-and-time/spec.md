# Spec Delta: sky-and-time

## Purpose

Defines world time and the calendar, the sun and moon in the sky, daylight and night
for the renderer, and how the seasons show in the landscape.

## ADDED Requirements

### Requirement: World time
The world SHALL keep game time in seconds, advancing every tick by the tick's real time scaled to a game day of 40 real minutes by default, and SHALL give its date as year, season (4 of 12 days), day and clock.

#### Scenario: A calendar of days and seasons
- **WHEN** dates are read at the start, later in a year, before the beginning and with a day length of zero
- **THEN** the world starts on the first spring day at 08:00, a later time gives its year, season, day, hour, minute and fractions, times before the start read as the start, and a day length of zero falls back to one game day per real second

#### Scenario: Time passes in the game
- **WHEN** the game runs for some ticks and a script sets the time
- **THEN** the date advances by 36 game seconds per real second and jumps to the time set

### Requirement: Sun and moon
The sun SHALL cross the sky once per day with a declination from the axial tilt and the time of year, and a moon SHALL circle every 12 days, new beside the sun and full opposite it, both as directions over any place.

#### Scenario: The sun crosses the sky
- **WHEN** the sun is placed at noon, midnight and sunrise on the equator at the equinox, at other longitudes, at midsummer at the tropic and near the poles, and without tilt
- **THEN** it stands overhead at noon and below at midnight, rises at 6 o'clock, comes earlier to the east, stands overhead at the tropic at midsummer, never sets at 80° N then while never rising at 80° S, and gives the southern hemisphere the opposite season

#### Scenario: The moon and its phases
- **WHEN** the moon is placed at the start and half a cycle later
- **THEN** it is new and dark beside the sun, then full, lit and opposite the sun, and up at midnight

#### Scenario: Deterministic sky
- **WHEN** sun, moon, daylight and season samples across times and places are hashed natively and in WebAssembly
- **THEN** the hash matches the recorded golden hash

### Requirement: Daylight
With daylight on, the renderer SHALL take its sky colours, sun, light and ambient from the sky above the camera: a blue day, an orange twilight, a dark night lit by the moon by its phase, with stars that turn with the sky and the moon's disc.

#### Scenario: Day, twilight and night
- **WHEN** daylight is computed at noon, at sunset and at midnight with a full and a new moon
- **THEN** noon has a bright blue sky and a sun light from above, sunset an orange horizon, midnight a dark sky with stars and the light from the moon, brighter under a full moon than a new one

#### Scenario: The night sky is drawn
- **WHEN** the flight scene is rendered at noon and at midnight looking up
- **THEN** the noon sky is bright and blue, and the midnight sky is dark with scattered bright stars

### Requirement: Seasons in the landscape
Plants SHALL take the colours of the season at the camera (half a year apart south of the equator), and snow SHALL cover flat ground of chunks whose season is below freezing at their height.

#### Scenario: Snow in winter
- **WHEN** the snow amount is computed for a temperate place in summer and winter, a high place, and a tropical place
- **THEN** there is none in summer, some in winter, more higher up, and none in the tropics

### Requirement: Time and sky from Lua
Scripts SHALL read the date and the sky over a position, set the time, and turn daylight on or off; the flight HUD SHALL show the day, season and time.

#### Scenario: Day and night from Lua
- **WHEN** a script reads the time and sky, sets noon and midnight, and flies with daylight on
- **THEN** the date and sky follow the time set, the sun is up at noon and down at midnight, and the HUD shows the season and the clock
