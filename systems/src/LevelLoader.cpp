#include "LevelLoader.h"

#include "ClueTrigger.h"
#include "Coin.h"
#include "HiddenWall.h"
#include "Level.h"
#include "TreasureRoom.h"
#include "TriggerWall.h"
#include "Wall.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

Level* LevelLoader::loadFromJson(const QString& filePath, QJsonArray* outClues)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return nullptr;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return nullptr;
    }

    const QJsonObject root = doc.object();

    Level* level = new Level();
    level->setName(root.value("name").toString("Unnamed Level"));
    level->setSize(root.value("width").toInt(0), root.value("height").toInt(0));
    level->setTimeLimit(root.value("timeLimit").toInt(0));
    level->setSpawn(root.value("spawnX").toInt(0), root.value("spawnY").toInt(0));

    const int treasureX = root.value("treasureX").toInt(-1);
    const int treasureY = root.value("treasureY").toInt(-1);
    if (treasureX >= 0 && treasureY >= 0) {
        level->setTreasureRoom(new TreasureRoom(treasureX, treasureY));
    }

    const QJsonArray walls = root.value("walls").toArray();
    for (const QJsonValue& value : walls) {
        const QJsonArray xy = value.toArray();
        if (xy.size() < 2) {
            continue;
        }
        level->addWall(new Wall(xy.at(0).toInt(), xy.at(1).toInt()));
    }

    const QJsonArray hiddenWalls = root.value("hiddenWalls").toArray();
    for (const QJsonValue& value : hiddenWalls) {
        const QJsonArray xy = value.toArray();
        if (xy.size() < 2) {
            continue;
        }
        level->addHiddenWall(new HiddenWall(xy.at(0).toInt(), xy.at(1).toInt()));
    }

    const QJsonArray triggerWalls = root.value("triggerWalls").toArray();
    for (const QJsonValue& value : triggerWalls) {
        const QJsonObject tw = value.toObject();
        const int x = tw.value("x").toInt();
        const int y = tw.value("y").toInt();

        QVector<QPoint> opens;
        const QJsonArray opensWalls = tw.value("opensWalls").toArray();
        for (const QJsonValue& openValue : opensWalls) {
            const QJsonArray xy = openValue.toArray();
            if (xy.size() < 2) {
                continue;
            }
            opens.push_back(QPoint(xy.at(0).toInt(), xy.at(1).toInt()));
        }

        level->addTriggerWall(new TriggerWall(x, y, opens));
    }

    const QJsonArray coins = root.value("coins").toArray();
    for (const QJsonValue& value : coins) {
        const QJsonObject coin = value.toObject();
        level->addCoin(new Coin(
            coin.value("x").toInt(),
            coin.value("y").toInt(),
            coin.value("value").toInt(100)));
    }

    const QJsonArray clues = root.value("clues").toArray();
    if (outClues) {
        *outClues = clues;
    }

    for (const QJsonValue& value : clues) {
        const QJsonObject clue = value.toObject();
        if (clue.value("condition").toString() != "position") {
            continue;
        }

        const int x = clue.value("triggerX").toInt();
        const int y = clue.value("triggerY").toInt();
        const QString text = clue.value("text").toString();

        level->addClueTrigger(new ClueTrigger(x, y, text));
    }

    return level;
}

QStringList LevelLoader::getAvailableLevels(const QString& levelsDir)
{
    QDir dir(levelsDir);
    const QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);

    QStringList result;
    result.reserve(files.size());
    for (const QString& file : files) {
        result.push_back(dir.absoluteFilePath(file));
    }

    return result;
}

