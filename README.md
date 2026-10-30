# Corner Shop Quest (C++)

A 2D top-down adventure RPG + family business game for children (working title). You and your
sibling set out to save the family's corner shop, exploring, making friends and learning how
money, work and people really work. Full design: [docs/GAME_DESIGN.md](docs/GAME_DESIGN.md).

C++20, built with the MSYS2 UCRT64 toolchain, raylib and jsoncpp.

**Current state: Phase 4 - Chapters 1 to 3.** Four areas (town, market square, Green Hill Farm,
Riverside City by bus); the pushcart, market stall, corner shop and a second city shop run by a
manager; staff with wages; the Town Bank with savings, interest and loans; bulk buying; weekly
rent; a MegaMart price war; fourteen quests; eight encounters; a "Their Side" scene; Festival Day;
the Life Wheel and chapter-end screens; day/night cycle with health; synthesized music and sound
effects; settings; and saving. Graphics and audio are generated in code (no asset files yet).

## How to play

| Key | Action |
|---|---|
| WASD / arrows | Walk (hold Shift to run) |
| E / Space / Enter | Talk, use doors and the pushcart, advance dialogue |
| 1-9 or mouse | Pick a choice |
| J (or Tab) | Journal: quests, bag, skills, people, Life Wheel |
| M | Music on / off |
| Esc | Pause menu (also music, sound effects and fullscreen settings) |

Look for gold **!** markers over people with something new to say. Walk east along the road to
reach the market square, or west to Green Hill Farm. From Chapter 3 a bus outside MegaMart goes
to Riverside City. A day runs from 08:00 to 20:00; go home to have dinner and sleep. Going
home early restores health, staying out until dark costs it. The game saves every night.

## Layout

```
CMakeLists.txt        Top level: C++ standard, options, dependencies, subdirectories
CMakePresets.json     debug / release presets pinned to C:/msys64/ucrt64 (g++, Ninja)
cmake/                Warning flags, DLL copy script
engine/               Static library, genre-agnostic
  include/engine/       Application (fixed-timestep loop), Scene, SceneManager, Ui (widgets),
                        Audio (a tiny synthesizer: sounds and music built from notes)
game/
  src/logic/          game_logic library - no rendering, unit-tested
                        GameState, Content (JSON loading), Rules (conditions, effects, day cycle,
                        Life Wheel), ShopSim (customers), Selling (a session at a venue, staff),
                        Encounter (social challenge rules), Bank (savings, loans), SaveGame
  src/scenes/         Title, World, Dialogue, Supplier, Sell, Encounter, Bank, Journal, Pause,
                      DaySummary, ChapterEnd
  src/Flow.*          Glue between scenes: new game, events, actions, day cycle
  src/Visuals.*       Palette and procedural drawing of characters, stalls, Life Wheel
  src/GameAudio.*     The game's sound effects, music loop, and settings (settings.json)
  src/main.cpp        Entry point and developer start options
assets/
  data/               All content as JSON: events, characters, quests, items, venues, suppliers,
                      encounters, start state and economy settings
  maps/               One JSON file per area (town, market, farm, city): tiles, buildings, spots,
                      exits
tests/                Unit tests for the logic library (plain asserts, run with CTest)
docs/                 Game design document
```

### Writing content

Story content needs no C++: edit the JSON in `assets/data/` and `assets/maps/`. Events have
lines, choices, conditions (`when`, `requires`, `show_when`), effects, follow-ups (`then`),
delayed consequences (`later`) and memories. New areas are new map files connected by `exits`.
The unit tests load the real content and report broken references, so run them after editing.

## Prerequisites (MSYS2 UCRT64)

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-raylib mingw-w64-ucrt-x86_64-jsoncpp
```

The presets expect MSYS2 at `C:/msys64`. If yours is elsewhere, copy `CMakePresets.json` to a
`CMakeUserPresets.json` (git-ignored) with your paths.

## Build, test and run

From PowerShell or the UCRT64 shell, in this folder:

```bash
C:/msys64/ucrt64/bin/cmake.exe --preset debug
C:/msys64/ucrt64/bin/cmake.exe --build --preset debug
C:/msys64/ucrt64/bin/ctest.exe --preset debug
build/debug/game/AAAGame.exe
```

Use `release` instead of `debug` for an optimized build. Saves go to `saves/slot1.json` next to
the exe.

### Developer options

Jump straight to a screen, optionally saving a screenshot of frame N and quitting:

```bash
build/debug/game/AAAGame.exe --start sell --screenshot shot.png --frames 60
```

`--start` accepts `town`, `park`, `evening`, `market`, `festival`, `exit-test`, `supplier`,
`sell`, `sell-run`, `sell-results`, `stall`, `stall-run`, `stall-results`, `ch2`, `shop`,
`shop-results`, `bank`, `farm`, `farm-supplier`, `ch3`, `city`, `city-market`, `ch3-evening`,
`encounter[:<id>]`, `journal` /
`journal:<0-4>`, `summary`, `chapter-end`, `title`, `continue`, `dialogue:<event id>` and
`choices:<event id>`. Dev starts never write the save file, and screenshot runs stay silent.

## Standalone package

```bash
C:/msys64/ucrt64/bin/cmake.exe --install build/release --prefix dist
```

`dist/` then contains the exe, every DLL it needs, and `assets/`, so it runs on a PC without MSYS2.
The UI uses the Segoe UI font from Windows; drop `ui.ttf` / `ui-bold.ttf` into `assets/fonts/`
to use your own.
