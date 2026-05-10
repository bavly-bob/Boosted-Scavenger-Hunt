#pragma once

enum class GameState {
    START,
    PLAYING,
    PAUSED,
    GAME_OVER,
    WIN
};

// Session lifecycle states — authoritative on the host, mirrored on the joiner.
// Separate from GameState so rendering logic is not entangled with networking.
// Note: no explicit underlying type — avoids cstdint dependency in the moc
// compilation path where header resolution uses 8.3 short-form paths.
enum class SessionState {
    WAITING_FOR_PLAYERS              = 0,  // host is listening, no peer yet
    LOADING                          = 1,  // peer connected; level loading in progress
    READY                            = 2,  // both sides loaded (reserved for future 3-way handshake)
    PLAYING                          = 3,  // active gameplay — Move packets accepted
    PLAYER_DEAD                      = 4,  // death detected; brief transitional state
    WAITING_FOR_RESTART_CONFIRMATION = 5,  // host waiting for all RestartReady ACKs
    RESTARTING                       = 6,  // all ACKs collected; level reload in flight
    DISCONNECTED                     = 7,  // peer dropped unexpectedly
    SESSION_CLOSED                   = 8   // host explicitly closed the session
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
