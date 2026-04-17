#pragma once

#include <QJsonArray>
#include <QString>
#include <QStringList>

class Level;

class LevelLoader {
public:
    static Level* loadFromJson(const QString& filePath, QJsonArray* outClues = nullptr);
    static QStringList getAvailableLevels(const QString& levelsDir);
};

