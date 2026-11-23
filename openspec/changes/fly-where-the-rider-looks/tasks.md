## 1. The look

- [x] 1.1 Up is up: the rider's view and the walker's look take the look's y as up (the setting still inverts)
- [x] 1.2 `camera.steer`: turn and climb from where the rider looks; `camera.follow`: the view held as the carpet turns
- [x] 1.3 main.lua reads the look before the flight and adds its steering to the flight input (not in VR, not on foot)

## 2. The carpet's speed

- [x] 2.1 Speed set by forward while it is held; letting go slows at 40 m/s², the brake at 60 m/s²; boost alone still flies

## 3. Words and tests

- [x] 3.1 The help and the first flight hint: look where to fly, hold forward to go
- [x] 3.2 Unit tests: speeds, letting go, looking around (up is up), turning to the look, hovering
- [x] 3.3 Scenarios that flew on one push of forward hold it; the replay fixture recorded again
