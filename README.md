# CSC 481/581 Milestone 3 - Individual Game (Sabah Afroz)

A side-scrolling princess platformer built on Team 8's game engine.
Team repository: https://github.com/mglenn001/CSC481-581Project

The game is built on the team's component-based game object model
(`GameObject` + components). Every object in the level, including the hidden
spawn points, death zones and scroll boundaries, is a `GameObject`.

## Build and run (macOS / Linux)
Requires CMake, SDL3 and SDL3_image.

    mkdir build && cd build
    cmake ..
    make
    ./game        # run from the build folder (assets are loaded from ../assets)

## Controls
| Key | Action |
|-----|--------|
| A / D | move left / right |
| Shift | run |
| W | jump |
| P | pause |
| - / = | slow down / speed up the game clock (0.5x, 1x, 2x) |
| F1 | show hidden objects (spawn points, death zones, scroll boundaries) |
| Esc | quit |

## Requirements checklist (Milestone 3, Part 2)
- Player character: princess, controlled with the keyboard (A/D/W, Shift to run).
- Static platforms with unique position/size/look: textured ground segments, a purple platform, a teal platform, a small textured platform.
- Moving platforms with three patterns: horizontal, circular, vertical.
- Hidden spawn points: 3 (no Renderable component, so never drawn).
- Hidden death zones: 2 pits plus a safety net, and a visible skull enemy that is also a death zone. Touching one sends the player to a spawn point.
- Side-scrolling boundaries: hidden left and right boundary objects that scroll the scene when the player collides with them.
- Extras: blocking crates and totem, 10 collectible stars, parallax clouds, goal portal, pause and time scaling.

## Source layout
- `src/GameObject.h`, `Component.h`, `Components.h`: the team's component-based object model.
- `src/GameComponents.h`: components added for this game (movement, spawn point, death zone, scroll boundary, goal, blocker, collectible, parallax, tile, color).
- `src/LevelObjects.h`: factory functions that build each kind of object from components.
- `src/GameLogic.h`: the game rules as systems over GameObjects, plus `buildLevel`.
- `src/Game.cpp`: window, input, drawing.

## Credits
Art (princess, platforms, crate, star, bush, cloud, background) is from my Milestone 1 project. Skull, portal and totem art are from the team assets.
AI assistance is disclosed in my written report.

## Tests
`make logic_test bot` then `./logic_test` (checks landing, moving platforms, scrolling, death zones, blockers, stars, respawn) and `./bot` (an automatic player that must reach the goal).
