# audio-playback Specification

## Purpose
Defines how sounds and music from packages are played: decoding, mixing, volume groups and
the platform quirks of browser audio, with silent but consistent behaviour when no device exists.

## Requirements

### Requirement: Sound playback from packages
Scripts SHALL be able to play WAV and OGG Vorbis files from packages by path, with volume, pitch and looping options, and receive a handle that can stop the sound or change its volume.

#### Scenario: Play and stop a loop
- **WHEN** a script plays a looping sound and stops it after 2 s
- **THEN** the sound is audible for 2 s and then silent, and its handle reports `stopped`

### Requirement: Mixing
The mixer SHALL play at least 32 voices simultaneously. It SHALL apply master, music and effects volume groups from the user configuration, and SHALL prevent clipping by limiting the mixed output.

#### Scenario: Many voices without clipping
- **WHEN** 32 full-volume sounds play at once
- **THEN** all 32 are mixed and no output sample exceeds full scale

#### Scenario: Music volume group
- **WHEN** the user sets the music volume to 0
- **THEN** sounds in the music group are silent and effect sounds are unaffected

### Requirement: Streamed music
Music files SHALL be decoded incrementally while playing, so that memory use for a track does not grow with its length.

#### Scenario: Long track
- **WHEN** a 10-minute OGG music track plays
- **THEN** the decoder's memory stays below 1 MiB for that track

### Requirement: Browser audio unlock
On the web the engine SHALL start audio output at the first user gesture. Music and looping sounds requested earlier SHALL start then. One-shot sounds requested before the gesture SHALL be dropped.

#### Scenario: Music requested before interaction
- **GIVEN** the title script starts music before any user input
- **WHEN** the player first clicks or touches the page
- **THEN** the music starts playing

### Requirement: Silent operation without a device
In headless mode, or when no audio device is available, playback calls SHALL succeed and the mixer SHALL advance voices in simulation time without output, so that scripts behave identically.

#### Scenario: Headless playback
- **WHEN** a script plays a 1-second one-shot sound in headless mode
- **THEN** its handle reports `playing` and reports `stopped` 60 ticks later
