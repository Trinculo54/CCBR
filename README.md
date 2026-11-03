<h1 align="center" id="title">CCBR</h1>

**Can't Crash, Busy Racing.** A mod for **Forza Horizon 2** on PC that makes your car a ghost for the length of a speed zone. From the moment the zone starts until it ends, you drive straight through traffic and Drivatars instead of into them, and every other car turns see-through, so you can see what you're passing through. When the zone ends, collision comes back as it was.

It's a single DLL, built on the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform). The game's files are never touched, so removing it puts everything back the way it was.

## Features

**A ghost car in speed zones.** As a speed zone starts, your car loses its collision with other cars, traffic and Drivatars alike. The scenery is still solid. It uses the game's own collision presets, the same kind the game uses for its own ghosted cars, so nothing else about the car changes.

**See-through traffic.** While your car is a ghost, every other car is see-through, near and far, including the ones the game brings onto the road further ahead while you drive. How solid they look is up to you, from almost invisible to solid. When the game takes a car away for a split second (it does so when it recycles one further down the road), the car stays as it is instead of blinking out.

**Switches in the game.** Pause > Settings > MODS has CCBR's switches: the ghost itself, see-through traffic, and a slider for how solid the other cars look. The switches count from the next speed zone; the slider applies straight away. Every change is saved to `CCBR.ini`.

## Install

You'll need Forza Horizon 2 running on PC through Azimuth (from Azimuth 1.0.3, the platform is the `DWrite.dll` in this zip). CCBR doesn't need Forza Horizon 2 Extra Options.

1. Close Azimuth.
2. Download the latest zip from [Releases](https://github.com/Trinculo54/CCBR/releases) and extract it into your Azimuth folder, the one with `azimuth.exe`. You should end up with `d3dcompiler_47.dll` and `DWrite.dll` next to `azimuth.exe`, and `fh2-mods\CCBR.dll`.
3. Start the game and drive into a speed zone.

### Updates

CCBR checks GitHub for a newer release at most every 6 hours, and the title screen says so after "PRESS (A) TO START" when there is one. `CheckUpdates=0` in `CCBR.ini` turns that off. The version you have is in `fh2-mods\platform.log` (`[CCBR] 1.0.0 registered`) and in the DLL's file properties.

### Using it with other FH2 mods

`d3dcompiler_47.dll` and `DWrite.dll` are the same file: the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform), the shared base for FH2 mods. Azimuth 1.0.3 loads `DWrite.dll`, older versions `d3dcompiler_47.dll`, and where both load only one of them runs. It loads every mod in the `fh2-mods` folder and lets them hook the game without stepping on each other. Each mod's zip ships the platform it was built with, and older ones only have `d3dcompiler_47.dll`. So extract CCBR last, or extract it again after installing a mod with an older platform. [WreckingBall](https://github.com/Trinculo54/WreckingBall), [Achievements](https://github.com/Trinculo54/fh2-achievements), [WheresMyBuddy](https://github.com/Trinculo54/WheresMyBuddy) and [FH2 Universal Radio](https://github.com/Trinculo54/FH2-Universal-Radio) are built on it too.

CCBR's zip has platform 2.8.0, and CCBR runs on 2.2.0 or later. The MODS page in the game comes with platform 2.3.0, its slider with 2.4.0 and the pause menu's MODS tab with 2.5.0; on an older platform the ghost and see-through traffic still work, and their settings are in the ini only. If another mod's zip puts an older platform back, the title screen says "Old FH2 Mod Platform: reinstall CCBR" under "PRESS (A) TO START". With a platform older than 2.2.0, CCBR doesn't start at all, and `platform.log` says it "needs FH2 Mod Platform 2.2.0 or newer". Either way, extract this zip again.

**Playing with a racing wheel.** [fh2wheel](https://github.com/iamAzoDx/fh2wheel) ships its own `D3DCOMPILER_47.dll`, and only one can sit next to `azimuth.exe`. Install fh2wheel first, then extract CCBR's zip and let its `d3dcompiler_47.dll` overwrite fh2wheel's. Keep `fh2wheel.dll` and `fh2wheel-config.exe` next to `azimuth.exe`: the platform (2.6.1 or later) loads fh2wheel itself, and it's listed on the MODS tab. Azimuth 1.0.3 doesn't load any `d3dcompiler_47.dll`, so fh2wheel's own loader doesn't run there, but the platform's `DWrite.dll` still loads `fh2wheel.dll`.

### On Linux (Wine or Proton)

On Azimuth 1.0.3 the platform is `DWrite.dll`, and Wine has its own copy of that: if `fh2-mods\platform.log` never appears, add `dwrite` to the DLL override, `WINEDLLOVERRIDES="winmm,dwrite=n,b"`. Getting FH2 mods running on Linux in general is covered in the platform's [Linux notes](https://github.com/Trinculo54/FH2-Mod-Platform/blob/main/docs/linux.md).

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

Delete `fh2-mods\CCBR.dll` and `fh2-mods\CCBR.ini`. The next time the game starts, the platform takes CCBR's rows off the MODS page, and about 15 seconds in it deletes CCBR's copy of `collidablepresets.xml` from the game's `mediaoverride` folder. If you don't use any other FH2 mods, start the game once more after deleting CCBR, then delete `d3dcompiler_47.dll`, `DWrite.dll` and the `fh2-mods` folder too. Nothing else was changed.

## Known limitations

- **Speed zones only.** Offline, the game gives no warning before a speed trap: the only event it sends is the one when you've already passed it. So your car keeps its collision through speed traps.
- **Offline only.** Online, the ghost would only exist on your own screen, and other players would still hit your car. When the game is started from FH2MP's launcher, platform 2.8.0 doesn't start CCBR (its MODS tab has a "Load when playing online" switch for it, off to start with); with an older platform, CCBR sees FH2MP's session itself and stays off.
- **Zone scores.** A ghost makes speed zones easier, but offline the scores only go to your own save.
- **Game version.** CCBR is written for the 24 November 2015 build of Forza Horizon 2 that Azimuth runs. It checks the game's code before hooking anything, and on anything else it stays off rather than patch the wrong place.

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

That gives `build\CCBR.dll` and the platform's `build\platform\d3dcompiler_47.dll` and `DWrite.dll`. `scripts/build.sh` builds and puts the release zip's contents in `dist/`: the platform under both names, `fh2-mods/CCBR.dll`, a `README.txt` and the licences. GitHub Actions does the same on every push, checks that the platform is there under both names, and uploads the result as a build artifact. Pushing a tag like `v1.0.0` also creates a draft release with the zip attached, ready to be published. The version lives in `project(... VERSION x.y.z)` in `CMakeLists.txt`. Bump it before tagging: the tag build refuses to run if the two don't match, and it's what the mod compares against the latest GitHub release. The tests are in [tests/](tests/README.md).

## Troubleshooting

CCBR logs to `fh2-mods\platform.log`, as lines starting `[CCBR]`. A working launch has `[CCBR] ready (6 of 6 speed camera events): ghost in speed zones on, other cars see-through meanwhile` a few seconds in, and each zone adds `speed zone started: your car is a ghost` and `speed zone completed after ... s: collision back on`. If the game crashes, the platform saves a report in `fh2-mods\crashes` that says which mod, if any, was involved.

| Symptom | Fix |
|---|---|
| No `[CCBR]` lines at all | The platform isn't loading CCBR. Make sure `d3dcompiler_47.dll` and `DWrite.dll` are both next to `azimuth.exe` (Azimuth 1.0.3 only loads `DWrite.dll`) and `CCBR.dll` is inside `fh2-mods`. If there's no `platform.log` either, the platform itself isn't loading; on Linux, see the Wine note above. |
| `platform.log` says CCBR "needs FH2 Mod Platform 2.2.0 or newer", or the title screen says "Old FH2 Mod Platform: reinstall CCBR" | Another mod's zip put an older platform back. If `fh2-mods\platform-standby.log` is from your last launch, it names the older copy that ran instead of this zip's platform. (That file stays once it's written, so check its date.) Extract this zip again. |
| `[CCBR] ... doesn't run in online play` | The game was started from FH2MP's launcher, where CCBR stays off on purpose (see Known limitations). |
| `game code doesn't match at ...: off` | Your game exe isn't the one CCBR was made for (the 24 November 2015 build Azimuth runs), or another mod changed that code first. |
| `the game has no "ccbr" preset (the collidablepresets.xml override wasn't read: is mediaoverride working?)` | The ghost still works, with the game's own `intersection` preset. CCBR's preset, like the MODS page, comes through the `mediaoverride` folder. Platform 2.7.3 and newer make the game read that folder when Extra Options isn't loaded; with Extra Options, its `EnableMediaOverrides=1` in `FH2_ExtraOptions.ini` does it, so keep that on. The second line of `FH2_ExtraOptions.log` should start with `OK`. If it starts with `REFUSED` (for example "unsupported Azimuth build") or `DISABLED`, Extra Options changed nothing, so nothing reads the folder: install the Extra Options release made for your Azimuth version, or remove its `winmm.dll` so the platform reads the folder itself. |
| Your car stays solid in a zone | Look for the zone's lines in the log. Set `Debug=1`, drive the zone again and attach `platform.log` when you report it. |

## License

[GPLv3](LICENSE). The FH2 Mod Platform is MIT licensed and includes [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause), whose licence ships in the zip as `LICENSE-MinHook.txt`.

## Disclaimer

Unofficial fan-made mod; not affiliated with Playground Games, Turn 10, Microsoft or the Azimuth developers.
