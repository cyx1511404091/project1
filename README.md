# Maze Adventure FP

This project is a complete maze mini-game built with `C++`.
The checked-in runnable [MazeGame.exe](/D:/codex/1/MazeGame.exe) in this workspace is compiled with native Win32 graphics because EasyX was not available in the current build environment.

## Features

- Start screen with `LOW`, `MEDIUM`, and `HIGH` difficulty selection
- DFS random perfect maze generation with guaranteed solvable layout
- Double-buffered first-person maze presentation to prevent flashing
- First-person play with visible left and right arms
- Start cell in the top-left corner and goal door in the bottom-right corner
- Long-press movement and turning with strict wall collision
- Reachable stars placed in the maze, each worth `1` score
- Spend `5` score to unlock a partial route hint from the current position
- `H` to show or hide the purchased partial hint overlay
- Real-time step counter, timer, score, and purchased hint count
- Victory popup with total steps and total time
- `R` to regenerate the maze and reset the run
- `ESC` to exit the game
- Executable icon resource for the final game file

## Visual Style

- Sky: light blue
- Corridor walls: dark stone gray
- Floor: warm brown
- Player view: blue sleeves with visible arms
- Goal: door model
- Hint path: yellow
- Stars: gold

## Build

1. Use Windows with a C++ compiler that supports Win32 desktop apps.
2. Add [src/main.cpp](/D:/codex/1/src/main.cpp) to your project.
3. Add [MazeGame.rc](/D:/codex/1/MazeGame.rc) so the executable uses the custom icon.
4. Keep [maze_game.ico](/D:/codex/1/assets/maze_game.ico) under the `assets` folder.
5. Build and run.

## One-Click Launch

You can click [MazeGame.exe](/D:/codex/1/MazeGame.exe) directly to launch the game.
If you prefer a helper file, you can also double-click [RunMazeGame.bat](/D:/codex/1/RunMazeGame.bat), which prioritizes the root `MazeGame.exe`.

## Tunable Constants

You can change these values near the top of [src/main.cpp](/D:/codex/1/src/main.cpp):

- `kMoveIntervalMs`
- `kTurnIntervalMs`
- `kHintCost`

## Notes

The DFS generator creates a connected perfect maze, so the goal is always reachable. The hint is generated with BFS from the player's current location after purchasing it with score, and the current build uses a first-person corridor renderer rather than a full 3D engine.
