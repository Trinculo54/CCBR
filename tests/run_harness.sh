#!/usr/bin/env bash
# Runs the harness (tests/harness.cpp) against the real game, in a folder laid out like an Azimuth
# install. Needs a build with -DCCBR_TESTS=ON in build-test/ and two files from your own copy of the
# game (copy them somewhere first, rather than using the game's folder):
#   tests/run_harness.sh <forza_durango_release_final.exe> <GameTunableSettings.zip>
# On Windows it runs the harness as it is; with WINE=1 set, under Wine.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
b=$root/build-test
exe=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
zip=$2
winpath() { cygpath -w "$1" 2>/dev/null || winepath -w "$1"; }
run() { if [ -n "${WINE:-}" ]; then wine "$@"; else "./$1" "${@:2}"; fi; }
status=0
for phase in offline online; do
    dir=$(mktemp -d)
    mkdir -p "$dir/fh2-mods" "$dir/FH2/Mount/media/Stripped/StringTables"
    cp "$b/harness.exe" "$dir/azimuth.exe"
    cp "$b/platform/d3dcompiler_47.dll" "$dir/"
    cp "$b/CCBR.dll" "$dir/fh2-mods/"
    printf '[CCBR]\r\nGhost=1\r\nSeeThrough=1\r\nOthersAlpha=0.5\r\nHoldBlinks=1\r\nCheckUpdates=0\r\nDebug=1\r\n' \
        > "$dir/fh2-mods/CCBR.ini"
    cp "$exe" "$dir/FH2/Mount/forza_durango_release_final.exe"
    cp "$zip" "$dir/FH2/Mount/media/GameTunableSettings.zip"
    args=()
    if [ "$phase" = online ]; then
        # As FH2MP's launcher leaves it: FH2MP.dll among the mods, and its session file.
        : > "$dir/fh2-mods/FH2MP.dll"
        mkdir -p "$dir/data/bin"
        printf 'xuid=1\ntoken=test\n' > "$dir/data/bin/fh2mp-session.txt"
        args=(online)
    fi
    echo "==== $phase"
    (cd "$dir" && run azimuth.exe "$(winpath "$dir/FH2/Mount/forza_durango_release_final.exe")" "${args[@]}") ||
        status=1
    rm -rf "$dir"
done
exit $status
