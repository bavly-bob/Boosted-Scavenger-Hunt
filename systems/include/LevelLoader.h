#pragma once

#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QtGlobal>

struct ProceduralLootRules {
    double spawnFrequency = 0.0;
    int maxPerRun = 0;
    int triggerCountRequired = 2;
};

struct ProceduralTriggerRules {
    int min = 1;
    int max = 1;
};

struct ProceduralHiddenWallRules {
    double frequency = 1.0;
};

struct ProceduralGenerationRules {
    int startingTime = 180;
    ProceduralLootRules lootRoom;
    ProceduralTriggerRules generalTriggers;
    ProceduralHiddenWallRules hiddenWalls;
};

struct ProceduralGenerationContext {
    ProceduralGenerationRules rules;
    quint32 runSeedBase = 0u;
    int runIndex = 0;
    int levelIndex = 0;
    int lootRoomsSpawnedThisRun = 0;
};

class Level;

class LevelLoader {
public:
    static Level* loadFromJson(const QString& filePath, QJsonArray* outClues = nullptr);

    static Level* generateProcedural(int seed, int difficulty, QJsonArray* outClues = nullptr);
    static Level* generateProcedural(int seed,
                                     int difficulty,
                                     const ProceduralGenerationContext& context,
                                     QJsonArray* outClues = nullptr,
                                     bool* outLootRoomSpawned = nullptr);

    static QStringList getAvailableLevels(const QString& levelsDir);

    static bool isChamberFormat(const QString& filePath);
};
