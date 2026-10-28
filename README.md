<h1 align="center" id="title">CCBR</h1>

**Can't Crash, Busy Racing.** A mod for **Forza Horizon 2** on PC that makes your car a ghost for the length of a speed zone. From the moment the zone starts until it ends, you drive straight through traffic and Drivatars instead of into them, and every other car turns see-through, so you can see what you're passing through. When the zone ends, collision comes back as it was.

It's a single DLL, built on the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform). The game's files are never touched, so removing it puts everything back the way it was.

## Features

**A ghost car in speed zones.** As a speed zone starts, your car loses its collision with other cars, traffic and Drivatars alike. The scenery is still solid. It uses the game's own collision presets, the same kind the game uses for its own ghosted cars, so nothing else about the car changes.

**See-through traffic.** While your car is a ghost, every other car is see-through, near and far, including the ones the game brings onto the road further ahead while you drive. How solid they look is up to you, from almost invisible to solid. When the game takes a car away for a split second (it does so when it recycles one further down the road), the car stays as it is instead of blinking out.

**Switches in the game.** Pause > Settings > MODS has CCBR's switches: the ghost itself, see-through traffic, and a slider for how solid the other cars look. The switches count from the next speed zone; the slider applies straight away. Every change is saved to `CCBR.ini`.

## Install

1. Close Azimuth.
2. Download the latest zip from [Releases](https://github.com/Trinculo54/CCBR/releases) and extract it into your Azimuth folder, the one with `azimuth.exe`. You should end up with `d3dcompiler_47.dll` and `DWrite.dll` next to `azimuth.exe`, and `fh2-mods\CCBR.dll`.
3. Start the game and drive into a speed zone.

`d3dcompiler_47.dll` and `DWrite.dll` are the same file: the FH2 Mod Platform, which loads every mod in `fh2-mods`. Azimuth 1.0.3 loads `DWrite.dll`, older versions `d3dcompiler_47.dll`. Every mod built on the platform ships the same files, so it's fine to overwrite them with a newer copy.

### Updates

CCBR checks GitHub for a newer release at most every 6 hours, and the title screen says so after "PRESS (A) TO START" when there is one. `CheckUpdates=0` in `CCBR.ini` turns that off.

## Settings

`fh2-mods\CCBR.ini` is created on the first launch, with a comment on every key. Changes made in the file apply the next time you start the game; delete it to get the defaults back. The first three are on the MODS page too.

| Key | Default | What it does |
|---|---|---|
| `Ghost` | `1` | `0` = your car keeps its collision in speed zones |
| `SeeThrough` | `1` | `0` = the other cars stay solid while your car is a ghost |
| `OthersAlpha` | `0.5` | how solid the other cars look meanwhile, from `0.05` (almost invisible) to `1.0` (solid) |
| `HoldBlinks` | `1` | `0` = a car the game takes away for a split second blinks out, as it would without CCBR |
| `CheckUpdates` | `1` | `0` = don't check GitHub for a newer version |
| `Debug` | `0` | `1` = log every speed camera event, and what the other cars look like at the start of each zone, 3 seconds in and at its end (for bug reports) |

## Uninstall

Delete `fh2-mods\CCBR.dll` and `fh2-mods\CCBR.ini`. Delete `d3dcompiler_47.dll`, `DWrite.dll` and `fh2-mods` too if no other FH2 mods use them.

## Known limitations

- **Speed zones only.** Offline, the game gives no warning before a speed trap: the only event it sends is the one when you've already passed it. So your car keeps its collision through speed traps.
- **Offline only.** Online, the ghost would only exist on your own screen, and other players would still hit your car. When the game is started from FH2MP's launcher, CCBR stays off.
- **Zone scores.** A ghost makes speed zones easier, but offline the scores only go to your own save.

## How it works

The speed camera that runs a zone sends its events (zone started, completed, aborted) through the game's entity event queue. CCBR hooks the two functions that send them, and matches each event against the code that sends it, so it never mistakes another event for one of these. Your car is the one the game marks as the local player's.

The ghost is a collidable request on your car, the game's own way of turning a car's collision off and on (it uses it for its own ghosts and for cars loading in). CCBR adds a preset of its own, `ccbr` (collision off), to the game's `collidablepresets.xml`: it reads the game's file from `media\GameTunableSettings.zip` when it starts, adds one line and puts the result in `mediaoverride` through the platform. If that doesn't work, it uses the game's own `intersection` preset, which does the same.

For the other cars, CCBR caps the alpha the game hands each Drivatar's and traffic car's presentation every frame, which works whichever request the game puts last on a car. The game runs its AI cars as a pool of 29 and puts them back on the road further ahead as you drive, hiding each for a frame or so as it does; CCBR holds that hide back for up to 0.15 seconds for a car that was visible, so it doesn't blink out.

Every game address is checked against the game's code before anything is hooked, and if any of it doesn't match, CCBR stays off and says so in the log. The addresses and what they are are in the comments in `src/ccbr.cpp`.

## Build from source

With [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (clang: the mod relies on its MSVC-style SEH and GNU extensions together) and Ninja:

```
git clone --recurse-submodules https://github.com/Trinculo54/CCBR.git
cd CCBR
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build
```

That gives `build\CCBR.dll` and the platform's `build\platform\d3dcompiler_47.dll` and `DWrite.dll`. `scripts/build.sh` builds and puts the release zip's contents in `dist/`. The tests are in [tests/](tests/README.md).

## Troubleshooting

CCBR logs to `fh2-mods\platform.log`, as lines starting `[CCBR]`. A working launch has `[CCBR] ready (6 of 6 speed camera events): ghost in speed zones on, other cars see-through meanwhile`, and each zone adds `speed zone started: your car is a ghost` and `speed zone completed after ... s: collision back on`.

- **No `[CCBR]` lines at all:** the platform isn't loading. Check that `d3dcompiler_47.dll` and `DWrite.dll` are next to `azimuth.exe`; the first lines of `platform.log` say which version is running.
- **"game code doesn't match ... off":** your game exe isn't the one CCBR was made for (the 24 November 2015 build Azimuth runs), or another mod changed that code first.
- **Your car stays solid in a zone:** look for the zone's lines in the log. Set `Debug=1`, drive the zone again and attach `platform.log` when you report it.

## License

[GPLv3](LICENSE). The FH2 Mod Platform is MIT licensed and includes MinHook (BSD 2-Clause); the zip carries both licences.

## Disclaimer

Unofficial fan-made mod; not affiliated with Playground Games, Turn 10, Microsoft or the Azimuth developers.
