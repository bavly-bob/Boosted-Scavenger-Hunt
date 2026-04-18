#pragma once

#include <QString>
#include <QStringList>

struct InteractionResult {
    QStringList revealedClues;

    bool coinCollected = false;
    int coinsCollectedTotal = 0;
    int scoreDelta = 0;

    bool triggerActivated = false;
    bool wallOpened = false;
    bool treasureUnlocked = false;
    bool won = false;
};

