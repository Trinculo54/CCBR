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

  - Forza Horizon 2 running through Azimuth.


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
fh2-mods, and every mod built on it ships the same files, so it's fine to
overwrite them.


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
    FH2MP's launcher, CCBR stays off.


Troubleshooting
~~~~~~~~~~~~~~~

CCBR logs to fh2-mods\platform.log, as lines starting "[CCBR]". A working
launch has "[CCBR] ready (6 of 6 speed camera events): ghost in speed
zones on, ...", and each zone adds "speed zone started: your car is a
ghost" and "speed zone completed after ... s: collision back on". If
nothing happens and there's no "[CCBR]" line at all, the platform isn't
loading: check that d3dcompiler_47.dll and DWrite.dll are next to
azimuth.exe. To report a problem, set Debug=1, drive the zone again and
attach platform.log.


Uninstall
~~~~~~~~~

Delete fh2-mods\CCBR.dll and fh2-mods\CCBR.ini. Delete
d3dcompiler_47.dll, DWrite.dll and fh2-mods too if no other FH2 mods use
them.


License
~~~~~~~

GPLv3. Unofficial fan-made mod; not affiliated with Playground Games,
Turn 10, Microsoft or the Azimuth developers.
