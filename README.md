# EasyX Maze Game

This project is a complete maze mini-game built with `C++` and the `EasyX` graphics library.

## Features

- Random perfect maze generation using DFS
- Adjustable maze size in code, default is `25 x 25`
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
5. Build and run.

## Tunable Constants

You can change these values near the top of [src/main.cpp](/D:/codex/1/src/main.cpp):

- `kMazeRows`
- `kMazeCols`
- `kCellSize`
- `kMoveCooldownMs`

## Notes

The DFS generator creates a connected perfect maze, so the goal is always reachable. The BFS hint is recomputed from the player's current location whenever the player moves or toggles the hint.
