# Corner Shop Quest (C++)

A 2D top-down adventure RPG + family business game for children (working title). You and your
sibling set out to save the family's corner shop, exploring, making friends and learning how
money, work and people really work. Full design: [docs/GAME_DESIGN.md](docs/GAME_DESIGN.md).

C++20, built with the MSYS2 UCRT64 toolchain, raylib and jsoncpp.

**Current state: Phase 2 - Chapter 1 complete.** Two areas (town and market square), a family,
three friends, a rival and a grumpy customer, seven quests, the pushcart and market stall
businesses, four encounters, a "Their Side" scene, Festival Day, the Life Wheel and a chapter-end
screen, day/night cycle with health, and saving. Graphics are drawn in code (no art assets yet).

## How to play

| Key | Action |
|---|---|
| WASD / arrows | Walk (hold Shift to run) |
| E / Space / Enter | Talk, use doors and the pushcart, advance dialogue |
| 1-9 or mouse | Pick a choice |
| J (or Tab) | Journal: quests, bag, skills, people, Life Wheel |
| Esc | Pause menu |

Look for gold **!** markers over people with something new to say. Walk east along the road to
reach the market square. A day runs from 08:00 to 20:00; go home to have dinner and sleep. Going
home early restores health, staying out until dark costs it. The game saves every night.

## Layout

```
CMakeLists.txt        Top level: C++ standard, options, dependencies, subdirectories
CMakePresets.json     debug / release presets pinned to C:/msys64/ucrt64 (g++, Ninja)
cmake/                Warning flags, DLL copy script
engine/               Static library, genre-agnostic
  include/engine/       Application (fixed-timestep loop), Scene, SceneManager, Ui (widgets)
game/
  src/logic/          game_logic library - no rendering, unit-tested
                        GameState, Content (JSON loading), Rules (conditions, effects, day cycle,
                        Life Wheel), ShopSim (customers), Selling (a whole session at a venue),
                        Encounter (social challenge rules), SaveGame
  src/scenes/         Title, World, Dialogue, Supplier, Sell, Encounter, Journal, Pause,
                      DaySummary, ChapterEnd
  src/Flow.*          Glue between scenes: new game, events, actions, day cycle
  src/Visuals.*       Palette and procedural drawing of characters, stalls, Life Wheel
  src/main.cpp        Entry point and developer start options
assets/
  data/               All content as JSON: events, characters, quests, items, venues, encounters
  maps/               One JSON file per area (town, market): tiles, buildings, spots, exits
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
`sell`, `sell-run`, `sell-results`, `stall`, `stall-run`, `stall-results`,
`encounter[:<id>]`, `journal` / `journal:<0-4>`, `summary`, `chapter-end`, `title`,
`continue`, `dialogue:<event id>` and `choices:<event id>`. Dev starts never write the save file.

## Standalone package

```bash
C:/msys64/ucrt64/bin/cmake.exe --install build/release --prefix dist
```

`dist/` then contains the exe, every DLL it needs, and `assets/`, so it runs on a PC without MSYS2.
The UI uses the Segoe UI font from Windows; drop `ui.ttf` / `ui-bold.ttf` into `assets/fonts/`
to use your own.
