# The Browning African Safari: native Linux port

This is a native Linux (SDL2) version of *The Browning African Safari*
(Oquirrh Productions / Multimedia Holding LC, 1998, v1.02). It was rebuilt
from the original `SAFARI.EXE` using the debug symbols (`Safari.pdb`) that
shipped on the CD. Game logic, timing, random numbers and floating-point
behaviour follow the original. The Windows-only parts (SciTech MGL graphics,
Miles Sound System, DirectX) were replaced with SDL2.

**No game data is included.** You need your own copy of the CD, or an image
of it (`SAFARI.iso`).

## Install (Fedora)

```
sudo dnf install ./browning-safari-1.02-3.x86_64.rpm
```

If you'd rather build it on your own machine:

```
sudo dnf install rpm-build cmake gcc SDL2-devel
rpmbuild --rebuild browning-safari-1.02-3.src.rpm
sudo dnf install ~/rpmbuild/RPMS/x86_64/browning-safari-1.02-3*.rpm
```

## First start: importing the game data

Start **Browning African Safari** from the application menu. The first time,
the game looks for the original CD. If it can't find one, it opens a file
picker so you can choose `SAFARI.iso`. The data (about 130 MB) is copied
once to `~/.local/share/browning-safari/data`. After that, the CD or ISO is
no longer needed.

You can also do this from a terminal:

```
browning-safari --import ~/Downloads/SAFARI.iso     # an ISO image
browning-safari --import /run/media/$USER/SAFARI    # a mounted CD
```

Saved games, `options.dat` and the log `oquirrh.txt` go in
`~/.local/share/browning-safari`.

## Options

| option | |
|---|---|
| `--fullscreen` | start fullscreen (toggle any time with **Alt+Enter** or **F11**) |
| `--import PATH` | copy the data from an ISO, a mounted CD or a folder |
| `--data DIR` | use game data from another folder |
| `--8bit-sound` | use the CD's 8-bit / 11 kHz sound set instead of 16-bit / 22 kHz |
| `--nosound` | no audio |
| `--debug` | verbose log on stderr |

The window can be resized. The 640×480 picture is scaled up with sharp
pixels and keeps its 4:3 shape.

## Controls (same as the original)

* **Mouse**: everything. Move the pointer to the left or right edge of the
  screen to look around (in the hunt, the shop and the museum).
* **Hunting**:
  * **Left click** fires.
  * **Right-drag** (or the **arrow keys**) adjusts the iron sight or the bow
    sight.
  * **Right click** on the weapon icon switches weapons.
  * **Right click** on the sunscreen or bug-spray icon swaps between them.
* **L**: load a round. **B**: binoculars (while hunting).
* On the area map: **B**, **C**, **I**, **L**, **R**, **W** and **Z** pick an
  area.
* **Esc** skips a character's speech.
* The original cheats work too:
  * **Ctrl+4** in the outfitter's shop, with the pointer in the top-left
    corner, gives $1000.
  * **Ctrl+F11** while hunting resets the clock to 9:00.

Tip from the guide: don't go out without malaria pills.

## Changes from the original

* **Bug fix:** an area that held exactly two species never spawned any
  animals. The original worked out the species ratio with integer division,
  which always gave 0%. It now uses the same formula as the three-species
  case.
* **Bug fix:** the guide's remarks after a shot were off by one in the
  original: each remark played the next clip in the guide's list, so a
  clean kill could get "I hope we can find him" and a wounding shot "this
  could mean trouble" or "you missed it". Each shot now gets its own remark.
* **Bug fix:** a shot was tested against the scenery (bushes, rocks) of the
  wrong part of the panorama, the left end of it, instead of the part on
  screen. Shots at a clearly visible animal could be stopped by an
  invisible bush, and shots into a bush could hit what was behind it.
* **Bug fix:** the museum's scrolling loop started from the wrong hot spot.
  This one had no visible effect.
* The binoculars are always lowered at the start of a hunt. Before, they
  could stay "on" from a previous hunt, so the first click did nothing.
* Edge scrolling runs at a fixed rate of about 33 steps per second. The
  original scrolled once per frame, which would be far too fast on a modern
  PC.
* Closing the window (Alt+F4, the title-bar button, logging out, or "End
  process" in System Monitor) quits right away from any screen, even in
  the middle of a hunt or while someone is talking. Unsaved progress is
  saved first, the same as answering "Save" in the original's exit dialog.
  Rounds still in your gun go back in the box. The in-game Exit button
  still asks before quitting, as it did in 1998.
* Sound fixes. The original could lose track of which sound was playing on
  which channel. A finished sound could then point at a different sound,
  for example a looping one, and anything waiting for it to end would wait
  forever. A malformed sound file can no longer make the game read past
  the end of its data.
* Pressing Enter or Esc closes one-button message boxes, such as
  "Game Paused" / Resume.
* Pressing Esc to skip someone's speech works even if you pressed another
  key first.
* The main loop no longer keeps a CPU core busy at 100%, and the screen is
  redrawn at up to 60 frames per second.
* The original logged to `C:\OQUIRRH.TXT`. The port writes the log to
  `~/.local/share/browning-safari/oquirrh.txt`.

## Building from source

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/browning-safari --import /path/to/SAFARI.iso
```

## Legal

The game, its artwork, sound and music are © 1998 Multimedia Holding LC.
None of them are included here. This port is meant for owners of the
original CD to play it on a modern system.
