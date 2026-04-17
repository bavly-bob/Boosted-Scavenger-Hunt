#include "ClueManager.h"

#include <QJsonObject>

void ClueManager::loadClues(const QJsonArray& cluesArray)
{
    m_clues.clear();
    m_clues.reserve(cluesArray.size());

    for (const QJsonValue& value : cluesArray) {
        const QJsonObject clue = value.toObject();
        ClueEntry entry;
        entry.condition = clue.value("condition").toString();
        entry.coinsRequired = clue.value("coinsRequired").toInt(0);
        entry.triggerX = clue.value("triggerX").toInt(-1);
        entry.triggerY = clue.value("triggerY").toInt(-1);
        entry.text = clue.value("text").toString();
        entry.revealed = false;
        m_clues.push_back(entry);
    }
}

QStringList ClueManager::checkCoinClues(int coinsCollected)
{
    QStringList newlyRevealed;
    for (ClueEntry& clue : m_clues) {
        if (clue.revealed) {
            continue;
        }
        if (clue.condition != "coins") {
            continue;
        }
        if (coinsCollected < clue.coinsRequired) {
            continue;
        }

        clue.revealed = true;
        if (!clue.text.isEmpty()) {
            newlyRevealed.push_back(clue.text);
        }
    }
    return newlyRevealed;
}

QStringList ClueManager::checkPositionClue(int x, int y)
{
    QStringList newlyRevealed;
    for (ClueEntry& clue : m_clues) {
        if (clue.revealed) {
            continue;
        }
        if (clue.condition != "position") {
            continue;
        }
        if (clue.triggerX != x || clue.triggerY != y) {
            continue;
        }

        clue.revealed = true;
        if (!clue.text.isEmpty()) {
            newlyRevealed.push_back(clue.text);
        }
    }
    return newlyRevealed;
}

QStringList ClueManager::getRevealedClues() const
{
    QStringList revealed;
    for (const ClueEntry& clue : m_clues) {
        if (clue.revealed && !clue.text.isEmpty()) {
            revealed.push_back(clue.text);
        }
    }
    return revealed;
}

void ClueManager::reset()
{
    for (ClueEntry& clue : m_clues) {
        clue.revealed = false;
    }
}

