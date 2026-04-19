#pragma once

#include "Enums.h"
#include "LevelLoader.h"

#include <QHash>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct DifficultyProfile {
    QString id;
    Difficulty difficulty = Difficulty::NORMAL;
    ProceduralGenerationRules rules;
};

class DifficultyConfig {
public:
    static DifficultyConfig loadFromJson(const QString& filePath);
    static DifficultyConfig defaults();

    bool isLoaded() const;
    QString sourcePath() const;

    DifficultyProfile selectProfile(Difficulty difficulty, quint32 runSeedBase, int runIndex) const;

private:
    bool m_loaded = false;
    QString m_sourcePath;
    QHash<int, QVector<DifficultyProfile>> m_profilesByDifficulty;
};
