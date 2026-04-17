#pragma once

#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVector>

class ClueManager {
    struct ClueEntry {
        QString condition;
        int coinsRequired;
        int triggerX;
        int triggerY;
        QString text;
        bool revealed;
    };

    QVector<ClueEntry> m_clues;

public:
    void loadClues(const QJsonArray& cluesArray);

    QStringList checkCoinClues(int coinsCollected);
    QStringList checkPositionClue(int x, int y);

    QStringList getRevealedClues() const;
    void reset();
};

