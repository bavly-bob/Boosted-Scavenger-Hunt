#include "Enums.h"

#include <iostream>

int main()
{
    if (static_cast<int>(Direction::Up) == static_cast<int>(Direction::Down)) {
        std::cerr << "Direction enum values should be distinct\n";
        return 1;
    }
    if (static_cast<int>(AnimationState::Idle) == static_cast<int>(AnimationState::MovingLeft)) {
        std::cerr << "AnimationState enum values should be distinct\n";
        return 1;
    }
    if (static_cast<int>(GameState::PLAYING) == static_cast<int>(GameState::GAME_OVER)) {
        std::cerr << "GameState enum values should be distinct\n";
        return 1;
    }

    return 0;
}
