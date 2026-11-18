## ADDED Requirements

### Requirement: Bodies fall, collide and rest
The simulation SHALL keep spheres, capsules and boxes of a solid or powder registry material, with the mass from the material's density, refuse other sizes than 1 cm to 50 m and other materials, let them fall towards the planet's centre, collide with the ground and with each other with restitution, friction from both materials and rolling resistance, and bring them to rest.

#### Scenario: Bodies fall and come to rest
- **WHEN** a granite ball, an oak crate and a tipping log are dropped onto flat ground
- **THEN** the ball falls 4.9 m in the first second, and after ten seconds each lies on the ground at its own half height, the log on its side, all asleep with no motion left

#### Scenario: Friction holds or lets go
- **WHEN** a ball, a granite block and an ice block are set on a 24 degree granite slope
- **THEN** the ball rolls down spinning, the granite block stays where it is and the ice block slides down

#### Scenario: Bodies collide
- **WHEN** two balls meet head-on in the air, two crates are stacked, a slab carries a tipped box, and logs lie side by side with one across them
- **THEN** the struck ball flies on with the momentum kept, the stack stands with each crate on the other, the box lands on the slab rather than through it, and the logs come to rest with the third lying across the others

### Requirement: Sleeping bodies
A body at rest SHALL sleep and cost nothing until a push, a knock from a moving body, or a change to the ground or water under it wakes it; something settling on it SHALL NOT wake it.

#### Scenario: Sleeping bodies wake
- **WHEN** resting bodies are pushed, have a hole dug under them, or are hit by a rolling iron ball
- **THEN** the pushed one rises, the one over the hole falls into it, and the one hit is knocked along

### Requirement: Bodies in water
Water SHALL hold bodies up by the volume of them under its surface, slow them, and carry them along with its flow.

#### Scenario: Wood floats and stone sinks
- **WHEN** an oak beam, a pine ball and a granite ball are dropped into a pond, and a pine ball is put by a spring on flat ground
- **THEN** the beam floats about seven tenths under, the pine ball about half, the granite ball sinks to the bottom, and the ball by the spring is carried away from it

### Requirement: Projectiles and sweeps
Projectiles SHALL fly under gravity and drag and test each step's path against the ground and the bodies, stopping at the first contact and handing their momentum to the body struck; sweeps of a ray or sphere SHALL report the first body or ground touched.

#### Scenario: Projectiles never pass through
- **WHEN** a bolt at 300 m/s is shot at a board 4 cm thick, a stone is thrown up, and rays are cast down, through open air and from inside the ground
- **THEN** the bolt stops at the board's face and strikes it, the stone rises and comes down on the ground, and is forgotten ten seconds later, the rays find the ground, the board or nothing, and out-of-range projectiles are refused

### Requirement: Bodies and the active tier
Bodies outside the active tier SHALL stay frozen where they are, and the bodies SHALL be counted, listed near a place and removable.

#### Scenario: Bodies outside the active tier are frozen
- **WHEN** invalid bodies are asked for, one body is put far outside the active tier and one inside, and later the world changes
- **THEN** the invalid ones are refused, the far one waits in the air while the near one falls, the counts and lists agree, removed bodies are gone, and a new world starts with no bodies

### Requirement: Deterministic bodies
The same bodies and projectiles in the same world SHALL move exactly the same.

#### Scenario: Deterministic bodies
- **WHEN** the same mix of bodies and a projectile is run twice on a slope
- **THEN** both runs end with the same state hash

### Requirement: Bodies in the game
The engine SHALL run the bodies in the active tier and draw them in their material's colour; scripts SHALL spawn, read, push, remove, find and count bodies, launch and follow projectiles, and sweep rays; saved games SHALL keep the bodies.

#### Scenario: Bodies from Lua
- **WHEN** a scenario drops a boulder, a crate and a log by the carpet, pushes the crate, throws a stone down onto it and casts rays
- **THEN** they come to rest on the ground, the crate jumps when pushed, the stone strikes the crate's top, the rays find the ball or nothing, and invalid shapes, materials, projectiles, impulses and radii are refused

#### Scenario: Bodies are drawn
- **WHEN** a dozen bodies are dropped in front of the camera
- **THEN** each is drawn as a mesh

#### Scenario: Bodies are kept with the game
- **WHEN** a crate is dropped near the carpet, the game is saved, and the game starts again
- **THEN** the crate is back with its shape, size and material, and a save from before bodies loads with none

### Requirement: Bodies budget
A step of 200 awake bodies piling up SHALL take no more than 1.5 ms (median) in a desktop release build.

#### Scenario: Bodies budget
- **WHEN** 200 boulders, crates and logs tumble into a heap
- **THEN** most of them are awake with hundreds of contacts, and the median step takes 1.5 ms or less
