#pragma once

enum class GameState {
    START,
    PLAYING,
    PAUSED,
    GAME_OVER,
    WIN
};

enum class Difficulty {
    EASY,
    NORMAL,
    HARD
};

enum class Direction {
    None,
    Up,
    Down,
    Left,
    Right
};

enum class AnimationState {
    Idle,
    MovingUp,
    MovingDown,
    MovingLeft,
    MovingRight,
    Dying
};

enum class CellType {
    Empty,
    Wall,
    TriggerWall,
    HiddenWall,
    Coin,
    TreasureRoom,
    ClueTrigger,
    Player,
    Corridor,
    Chamber,
    OpenedSecretFloor
};
