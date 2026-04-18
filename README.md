# 🕵️‍♂️ Scavenger Hunt

A 2D top-down dungeon exploration and scavenger hunt RPG built with C++ and Qt6. 

Navigate through procedurally generated chambers and carefully crafted levels, collect scattered coins, decipher AI-generated clues, and unlock the hidden treasure vault before time runs out!

## ✨ Features

- **Dynamic Level Generation**: Uses BSP (Binary Space Partitioning) to procedurally generate unique dungeon layouts alongside pre-designed JSON levels.
- **Custom Tile-based Rendering**: Smooth sub-tile player movement, frame-based sprite animations, and a dynamic fog-of-war system.
- **Interactive Environment**: Discover hidden passages, interact with trigger walls to unlock new areas, and avoid patrolling enemies.
- **AI Hint System**: Integrates with a local LLM server to provide contextual, atmospheric clues based on your progress.
- **Modular Architecture**: Clean, decoupled systems separating UI rendering, game logic, and asset management.

## 🎮 How to Play

**Objective:**
Explore the dungeon and collect **3 coins** to break the seal. Once you have enough coins, find the hidden trigger mechanism (pressure plate) to open the stone walls and reach the treasure chamber!

**Controls:**
- **W, A, S, D** or **Arrow Keys**: Move the player
- **ESC**: Pause the game

## 🛠️ Prerequisites

To build and run the game locally, you will need:

- **C++17** compatible compiler (e.g., MinGW, MSVC, GCC)
- **CMake** (version 3.16 or higher)
- **Qt 6** (Core, Widgets, Gui, Network modules)

## 🚀 Build Instructions

1. **Clone the repository:**
   ```bash
   git clone <repository-url>
   cd ScavengerHunt
   ```

2. **Configure with CMake:**
   Create a build directory and generate the build files (example using MinGW).
   ```bash
   mkdir build
   cd build
   cmake .. -G "MinGW Makefiles"
   ```

3. **Build the project:**
   ```bash
   cmake --build . --parallel 4
   ```

4. **Run the game:**
   ```bash
   ./ScavengerHunt.exe
   ```

## 📁 Project Structure

The codebase is organized into clean, decoupled modules:

- `core/`: Fundamental game state, entity base classes, and core level logic.
- `gameplay/`: Specific game entities (`Player`, `Enemy`, `Coin`, `TriggerWall`, etc.) and animation controllers.
- `systems/`: Managers for assets (`SpriteManager`), level loading/parsing (`LevelLoader`), input, and clues.
- `ui/`: Qt-based rendering (`GameWindow`), HUD overlay, and menu screens.
- `assets/`: Image sprites and textures.
  - `assets/player/`: Directional player frames (`down|up|left|right|idle/frame_N.png`).
  - `assets/dungeon/tiles/`: Floor sheet (`dungeon_floor_tiles.png`).
  - `assets/dungeon/walls/`: Wall sheet (`dungeon_wall_tiles.png`).
  - `assets/dungeon/props/`: Props sheet (`dungeon_props.png`).
  - `assets/dungeon/lighting/`: Lighting overlays and glow textures.
- `levels/`: JSON files defining static and procedurally generated level parameters.

### Asset Layout Used By Runtime

```text
assets/
  player/
    down/frame_0.png ...
    up/frame_0.png ...
    left/frame_0.png ...
    right/frame_0.png ...
    idle/frame_0.png ...
  dungeon/
    tiles/
      dungeon_floor_tiles.png
    walls/
      dungeon_wall_tiles.png
    props/
      dungeon_props.png
    lighting/
      ...
```

## 👥 Credits & Team

- **Bavly**: Integration, Testing Lead, Bug Tracking, Project Management
- **Ali**: Game Logic, Movement, Rules, Target Interaction
- **Youssef**: GUI, Qt Interface, Buttons, Screens, Score Display
- **Abdallah**: OOP Structure, Classes, Inheritance, Architecture
- **Ahmed**: Scoring System, Game State, Win/Lose Logic
