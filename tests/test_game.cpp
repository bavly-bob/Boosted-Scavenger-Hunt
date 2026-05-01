#include "DifficultyConfig.h"
#include "Enums.h"

#include <iostream>

int main()
{
    const DifficultyConfig config = DifficultyConfig::loadFromJson("levels/difficulty_configs.json");
    if (!config.isLoaded()) {
        std::cerr << "Difficulty config failed to load\n";
        return 1;
    }

    const DifficultyProfile easy = config.selectProfile(Difficulty::EASY, 123u, 1);
    const DifficultyProfile hard = config.selectProfile(Difficulty::HARD, 456u, 2);

    if (easy.rules.startingTime <= 0 || hard.rules.startingTime <= 0) {
        std::cerr << "Invalid starting time in selected profiles\n";
        return 1;
    }
    if (hard.rules.generalTriggers.max < hard.rules.generalTriggers.min) {
        std::cerr << "Invalid trigger bounds in hard profile\n";
        return 1;
    }

    return 0;
}
