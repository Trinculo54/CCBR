CCBR (Can't Crash, Busy Racing)
===============================

A ghost car in speed zones for Forza Horizon 2 (Azimuth). From the moment
a speed zone starts until it ends, your car drives straight through
traffic and Drivatars, and they turn see-through, so you can see what
you're driving through. Then collision comes back. Offline only. The
game's files are never touched.

Project page and source: https://github.com/Trinculo54/CCBR


Requirements
~~~~~~~~~~~~

  - Forza Horizon 2 running through Azimuth (from Azimuth 1.0.3, the
    platform is the DWrite.dll in this zip). Extra Options isn't needed.


Install
~~~~~~~

1. Close Azimuth.
2. Extract this zip into your Azimuth folder (the one with azimuth.exe).
   You should end up with:
       d3dcompiler_47.dll            next to azimuth.exe
       DWrite.dll                    next to azimuth.exe
       fh2-mods\CCBR.dll
3. Start the game and drive into a speed zone.

d3dcompiler_47.dll and DWrite.dll are the same file: the FH2 Mod
Platform, the shared base for FH2 mods
(https://github.com/Trinculo54/FH2-Mod-Platform). Azimuth 1.0.3 loads
DWrite.dll, older versions d3dcompiler_47.dll. It loads every mod in
fh2-mods. Each mod's zip ships the platform it was built with, and older
ones only have d3dcompiler_47.dll, so extract this zip last, or extract
it again after installing a mod with an older platform.

This zip has platform 2.8.0, and CCBR runs on 2.2.0 or newer. The
switches in the game need 2.3.0 (the slider 2.4.0, the MODS tab 2.5.0);
without them, the settings are in CCBR.ini only. If the title screen
says "Old FH2 Mod Platform: reinstall CCBR", another mod's zip put an
older platform back. With a platform older than 2.2.0, CCBR doesn't
start at all, and platform.log says it "needs FH2 Mod Platform 2.2.0 or
newer". Either way, extract this zip again.

With a racing wheel (fh2wheel): install fh2wheel first, then extract
this zip and let its d3dcompiler_47.dll overwrite fh2wheel's. Keep
fh2wheel.dll and fh2wheel-config.exe next to azimuth.exe: the platform
loads fh2wheel.dll itself, on Azimuth 1.0.3 too.


In the game
~~~~~~~~~~~

Pause > Settings > MODS has CCBR's switches:

  Ghost car in speed zones    your car drives through other cars during a
                              speed zone (from the next zone)
  See-through traffic         the other cars are see-through meanwhile
                              (from the next zone)
  How solid traffic looks     100 is solid, lower is more see-through;
                              applies straight away

Changes are saved to CCBR.ini.


Settings (fh2-mods\CCBR.ini)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Created on first launch, with a comment next to each setting. Changes
made here apply the next time you start the game; delete the file to get
the defaults back.

  Ghost=1              0 = your car keeps its collision in speed zones
  SeeThrough=1         0 = the other cars stay solid
  OthersAlpha=0.5      how solid they look: 0.05 to 1.0
  HoldBlinks=1         0 = a car the game hides for a split second
                       blinks out instead of staying see-through
  CheckUpdates=1       0 = don't check GitHub for a newer version
  Debug=0              1 = log every speed camera event and what the
                       other cars look like (for bug reports)

When a newer version is out, the title screen says so after
"PRESS (A) TO START".


Good to know
~~~~~~~~~~~~

  - Speed zones only. Offline, the game gives no warning before a speed
    trap, so your car keeps its collision through traps.
  - Offline only. Online, the ghost would only exist on your own screen:
    other players would still hit your car. When you start the game from
    FH2MP's launcher, platform 2.8.0 doesn't start CCBR (the MODS tab has
    a "Load when playing online" switch for it, off to start with); with
    an older platform, CCBR sees FH2MP's session itself and stays off.
  - CCBR is made for the 24 November 2015 game exe that Azimuth runs. It
    checks the game's code first, and on anything else it stays off.


Troubleshooting
~~~~~~~~~~~~~~~

CCBR logs to fh2-mods\platform.log, as lines starting "[CCBR]". A working
launch has "[CCBR] ready (6 of 6 speed camera events): ghost in speed
zones on, ...", and each zone adds "speed zone started: your car is a
ghost" and "speed zone completed after ... s: collision back on". To
report a problem, set Debug=1, drive the zone again and attach
platform.log. If the game crashes, the platform saves a report in
fh2-mods\crashes.

  - No "[CCBR]" line at all: the platform isn't loading CCBR. Check that
    d3dcompiler_47.dll and DWrite.dll are both next to azimuth.exe
    (Azimuth 1.0.3 only loads DWrite.dll), and CCBR.dll is in fh2-mods.
    The first line of fh2-mods\platform.log says which platform version
    is running.
  - platform.log says CCBR "needs FH2 Mod Platform 2.2.0 or newer", or
    the title screen says "Old FH2 Mod Platform: reinstall CCBR":
    another mod's zip put an older platform back. If
    fh2-mods\platform-standby.log is from your last launch, it names the
    older copy that ran instead of this zip's platform. (That file stays
    once it's written, so check its date.) Extract this zip again.
  - "game code doesn't match at ...: off": the game exe isn't the one
    CCBR was made for, or another mod changed that code first.
  - "the game has no "ccbr" preset (the collidablepresets.xml override
    wasn't read: is mediaoverride working?)": the ghost still works,
    with the game's own "intersection" preset. CCBR's preset and its
    switches come through the mediaoverride folder. With Extra Options
    installed, keep EnableMediaOverrides=1 in FH2_ExtraOptions.ini, and
    check that the second line of FH2_ExtraOptions.log starts with OK.
    If it starts with REFUSED (for example "unsupported Azimuth build")
    or DISABLED, Extra Options changed nothing, so nothing reads
    mediaoverride: install the Extra Options release made for your
    Azimuth version, or remove its winmm.dll so the platform reads the
    folder itself. CCBR.ini works either way.
  - On Linux (Wine or Proton) with Azimuth 1.0.3, if
    fh2-mods\platform.log never appears, add dwrite to the DLL override:
    WINEDLLOVERRIDES="winmm,dwrite=n,b"


Uninstall
~~~~~~~~~

Delete fh2-mods\CCBR.dll and fh2-mods\CCBR.ini. The next time the
game starts, the platform takes CCBR's rows off the MODS page, and about
15 seconds in it deletes CCBR's copy of collidablepresets.xml from the
game's mediaoverride folder. If you don't use any other FH2 mods, start
the game once more after deleting CCBR, then delete d3dcompiler_47.dll,
DWrite.dll and the fh2-mods folder too. Nothing else was changed.


License
~~~~~~~

GPLv3. Unofficial fan-made mod; not affiliated with Playground Games,
Turn 10, Microsoft or the Azimuth developers.
