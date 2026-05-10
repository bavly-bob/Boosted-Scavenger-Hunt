#include "DifficultyConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace {

Difficulty parseDifficulty(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == "easy") {
        return Difficulty::EASY;
    }
    if (normalized == "hard") {
        return Difficulty::HARD;
    }
    return Difficulty::NORMAL;
}

QString difficultyName(Difficulty difficulty)
{
    switch (difficulty) {
    case Difficulty::EASY:
        return QStringLiteral("easy");
    case Difficulty::NORMAL:
        return QStringLiteral("normal");
    case Difficulty::HARD:
        return QStringLiteral("hard");
    }
    return QStringLiteral("normal");
}

ProceduralGenerationRules parseRules(const QJsonObject& object)
{
    ProceduralGenerationRules rules;
    rules.startingTime  = qMax(10, object.value("startingTime").toInt(rules.startingTime));
    rules.coinsRequired = qMax(1, object.value("coinsRequired").toInt(rules.coinsRequired));

    const QJsonObject loot = object.value("lootRoom").toObject();
    rules.lootRoom.spawnFrequency = qBound(0.0, loot.value("spawnFrequency").toDouble(rules.lootRoom.spawnFrequency), 1.0);
    rules.lootRoom.maxPerRun = qMax(0, loot.value("maxPerRun").toInt(rules.lootRoom.maxPerRun));
    rules.lootRoom.triggerCountRequired = qMax(2, loot.value("triggerCountRequired").toInt(rules.lootRoom.triggerCountRequired));

    const QJsonObject triggerRules = object.value("generalTriggers").toObject();
    rules.generalTriggers.min = qMax(1, triggerRules.value("min").toInt(rules.generalTriggers.min));
    rules.generalTriggers.max = qMax(rules.generalTriggers.min, triggerRules.value("max").toInt(rules.generalTriggers.max));

    const QJsonObject hiddenWalls = object.value("hiddenWalls").toObject();
    rules.hiddenWalls.frequency = qBound(0.0, hiddenWalls.value("frequency").toDouble(rules.hiddenWalls.frequency), 1.0);

    return rules;
}

DifficultyProfile defaultProfileFor(Difficulty difficulty)
{
    DifficultyProfile profile;
    profile.difficulty = difficulty;
    profile.id = difficultyName(difficulty) + QStringLiteral("_default");

    switch (difficulty) {
    case Difficulty::EASY:
        profile.rules.startingTime = 260;
        profile.rules.hiddenWalls.frequency = 0.40;
        profile.rules.generalTriggers.min = 1;
        profile.rules.generalTriggers.max = 2;
        profile.rules.lootRoom.spawnFrequency = 0.33;
        profile.rules.lootRoom.maxPerRun = 1;
        profile.rules.lootRoom.triggerCountRequired = 2;
        break;
    case Difficulty::NORMAL:
        profile.rules.startingTime = 200;
        profile.rules.hiddenWalls.frequency = 0.60;
        profile.rules.generalTriggers.min = 2;
        profile.rules.generalTriggers.max = 3;
        profile.rules.lootRoom.spawnFrequency = 0.25;
        profile.rules.lootRoom.maxPerRun = 1;
        profile.rules.lootRoom.triggerCountRequired = 2;
        break;
    case Difficulty::HARD:
        profile.rules.startingTime = 165;
        profile.rules.hiddenWalls.frequency = 0.80;
        profile.rules.generalTriggers.min = 2;
        profile.rules.generalTriggers.max = 4;
        profile.rules.lootRoom.spawnFrequency = 0.20;
        profile.rules.lootRoom.maxPerRun = 1;
        profile.rules.lootRoom.triggerCountRequired = 3;
        break;
    }

    return profile;
}

void appendProfile(QHash<int, QVector<DifficultyProfile>>& out, const DifficultyProfile& profile)
{
    out[static_cast<int>(profile.difficulty)].push_back(profile);
}

void parseProfilesArray(const QJsonArray& profiles, QHash<int, QVector<DifficultyProfile>>& out)
{
    for (const QJsonValue& value : profiles) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        DifficultyProfile profile;
        profile.difficulty = parseDifficulty(object.value("difficulty").toString("normal"));
        profile.id = object.value("id").toString();
        if (profile.id.isEmpty()) {
            profile.id = difficultyName(profile.difficulty) + QStringLiteral("_profile_%1")
                         .arg(out.value(static_cast<int>(profile.difficulty)).size() + 1);
        }
        profile.rules = parseRules(object);
        appendProfile(out, profile);
    }
}

void parseProfilesByDifficultyObject(const QJsonObject& root, QHash<int, QVector<DifficultyProfile>>& out)
{
    const QVector<QPair<QString, Difficulty>> mapping = {
        {QStringLiteral("easy"), Difficulty::EASY},
        {QStringLiteral("normal"), Difficulty::NORMAL},
        {QStringLiteral("hard"), Difficulty::HARD}
    };

    for (const auto& entry : mapping) {
        const QJsonArray profiles = root.value(entry.first).toArray();
        for (const QJsonValue& value : profiles) {
            if (!value.isObject()) {
                continue;
            }
            const QJsonObject object = value.toObject();
            DifficultyProfile profile;
            profile.difficulty = entry.second;
            profile.id = object.value("id").toString();
            if (profile.id.isEmpty()) {
                profile.id = entry.first + QStringLiteral("_profile_%1")
                             .arg(out.value(static_cast<int>(entry.second)).size() + 1);
            }
            profile.rules = parseRules(object);
            appendProfile(out, profile);
        }
    }
}

quint32 deterministicIndexSeed(Difficulty difficulty, quint32 runSeedBase, int runIndex)
{
    quint32 seed = runSeedBase;
    seed ^= static_cast<quint32>(static_cast<int>(difficulty) + 1) * 0x9e3779b9u;
    seed ^= static_cast<quint32>(runIndex + 1) * 0x85ebca6bu;
    return seed;
}

} // namespace

DifficultyConfig DifficultyConfig::loadFromJson(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Difficulty config not found at" << filePath << "- using defaults";
        return defaults();
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "Failed parsing difficulty config:" << error.errorString() << "- using defaults";
        return defaults();
    }

    DifficultyConfig config;
    const QJsonObject root = doc.object();
    parseProfilesArray(root.value("difficultyConfigs").toArray(), config.m_profilesByDifficulty);
    parseProfilesByDifficultyObject(root, config.m_profilesByDifficulty);

    for (Difficulty difficulty : {Difficulty::EASY, Difficulty::NORMAL, Difficulty::HARD}) {
        const int key = static_cast<int>(difficulty);
        if (config.m_profilesByDifficulty.value(key).isEmpty()) {
            appendProfile(config.m_profilesByDifficulty, defaultProfileFor(difficulty));
        }
    }

    config.m_loaded = true;
    config.m_sourcePath = filePath;
    return config;
}

DifficultyConfig DifficultyConfig::defaults()
{
    DifficultyConfig config;
    appendProfile(config.m_profilesByDifficulty, defaultProfileFor(Difficulty::EASY));

    DifficultyProfile easyVariant = defaultProfileFor(Difficulty::EASY);
    easyVariant.id = QStringLiteral("easy_loot_hunter");
    easyVariant.rules.startingTime = 280;
    easyVariant.rules.hiddenWalls.frequency = 0.35;
    easyVariant.rules.lootRoom.spawnFrequency = 0.45;
    appendProfile(config.m_profilesByDifficulty, easyVariant);

    DifficultyProfile easyVariant2 = defaultProfileFor(Difficulty::EASY);
    easyVariant2.id = QStringLiteral("easy_pathfinder");
    easyVariant2.rules.startingTime = 240;
    easyVariant2.rules.hiddenWalls.frequency = 0.50;
    easyVariant2.rules.generalTriggers.min = 2;
    easyVariant2.rules.generalTriggers.max = 2;
    appendProfile(config.m_profilesByDifficulty, easyVariant2);

    appendProfile(config.m_profilesByDifficulty, defaultProfileFor(Difficulty::NORMAL));
    appendProfile(config.m_profilesByDifficulty, defaultProfileFor(Difficulty::HARD));

    config.m_loaded = true;
    config.m_sourcePath = QStringLiteral("<defaults>");
    return config;
}

bool DifficultyConfig::isLoaded() const
{
    return m_loaded;
}

QString DifficultyConfig::sourcePath() const
{
    return m_sourcePath;
}

DifficultyProfile DifficultyConfig::selectProfile(Difficulty difficulty, quint32 runSeedBase, int runIndex) const
{
    const QVector<DifficultyProfile> profiles = m_profilesByDifficulty.value(static_cast<int>(difficulty));
    if (profiles.isEmpty()) {
        return defaultProfileFor(difficulty);
    }
    const quint32 seed = deterministicIndexSeed(difficulty, runSeedBase, runIndex);
    const int index = static_cast<int>(seed % static_cast<quint32>(profiles.size()));
    return profiles.at(index);
}
