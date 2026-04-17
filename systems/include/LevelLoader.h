#pragma once

#include <QJsonArray>
#include <QString>
#include <QStringList>

class Level;

class LevelLoader {
public:
    // Load a level from JSON. Automatically detects flat-grid vs chamber format.
    static Level* loadFromJson(const QString& filePath, QJsonArray* outClues = nullptr);

    // Procedurally generate a chamber-based level using a BSP algorithm.
    static Level* generateProcedural(int seed, int difficulty, QJsonArray* outClues = nullptr);

    static QStringList getAvailableLevels(const QString& levelsDir);

    // Returns true if the JSON root uses the new "chambers" format key.
    static bool isChamberFormat(const QString& filePath);
};
