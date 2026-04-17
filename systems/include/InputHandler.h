#pragma once

#include "Enums.h"

class InputHandler {
public:
    static Direction keyToDirection(int qtKey);
    static bool isPauseKey(int qtKey);
};

