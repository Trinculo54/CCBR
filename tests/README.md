# Tests

Build them with `-DCCBR_TESTS=ON`, in `build-test/`:

```
cmake -G Ninja -S . -B build-test -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCCBR_TESTS=ON
cmake --build build-test
```

Both need files from your own copy of the game. Copy them somewhere first rather than pointing at the game's folder.

## Unit test

`unit_test.exe [GameTunableSettings.zip]` compiles `src/ccbr.cpp` with the game's functions replaced by fakes that record each car's collidable requests and what alpha the game would draw it with. It covers the ghost starting and ending with the zone, the other cars' alpha cap and the held hides, cars going away mid-zone, a ghost no event ends, the settings and the MODS page's changes, the online check and the preset file. Given the game's `media\GameTunableSettings.zip`, it also adds the preset to the game's own `collidablepresets.xml`. It runs in a few seconds and prints `ALL PASSED`.

## Harness

`tests/run_harness.sh <forza_durango_release_final.exe> <GameTunableSettings.zip>` lays out a folder like an Azimuth install, with the platform, `CCBR.dll` and the game, and runs `harness.exe` there as `azimuth.exe`. It maps the game's exe as Azimuth does (none of the game runs), waits for CCBR to check and hook the game's code, then sends speed camera events through the hooked functions with a fake player's car. Twice: once offline, and once laid out as if FH2MP's launcher had started the game, where CCBR must stay off. Each prints `ALL PASSED`.

On Windows it runs the harness as it is; with `WINE=1` set, under Wine.
