# EasyX Maze Game

This project is a complete maze mini-game built with `C++` and the `EasyX` graphics library.

## Features

- Random perfect maze generation using DFS
- Adjustable maze size in code, default is `25 x 25`
- Launch directly into the maze after opening the program
- Executable icon resource for the final game file
- Start cell in the top-left corner and goal cell in the bottom-right corner
- Arrow-key player movement with strict wall collision
- Real-time step counter and timer
- Victory popup with total steps and total time
- `R` to regenerate the maze and reset the run
- `ESC` to exit the game
- BFS shortest-path hint from the current position to the goal
- `H` to show or hide the hint path without interrupting gameplay

## Visual Style

- Walls: dark gray
- Floor: light beige
- Player: blue
- Start: green
- Goal: red
- Hint path: yellow

## Build

1. Use Windows with Visual Studio installed.
2. Install and configure the EasyX graphics library.
3. Create an empty C++ project.
4. Add [src/main.cpp](/D:/codex/1/src/main.cpp) to the project.
5. Add [MazeGame.rc](/D:/codex/1/MazeGame.rc) to the project so the executable uses the custom icon.
6. Keep [maze_game.ico](/D:/codex/1/assets/maze_game.ico) under the `assets` folder in the project.
7. Build and run.

## One-Click Launch

After the project is built, you can click the generated `.exe` file directly to launch the game.
If you prefer a helper file, you can also double-click [RunMazeGame.bat](/D:/codex/1/RunMazeGame.bat), which searches common Visual Studio output folders such as `x64\Release` and `Debug`.

## Tunable Constants

You can change these values near the top of [src/main.cpp](/D:/codex/1/src/main.cpp):

- `kMazeRows`
- `kMazeCols`
- `kCellSize`
- `kMoveCooldownMs`

## Notes

The DFS generator creates a connected perfect maze, so the goal is always reachable. The BFS hint is recomputed from the player's current location whenever the player moves or toggles the hint.
