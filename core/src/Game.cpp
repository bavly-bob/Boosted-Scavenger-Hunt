#include "Game.h"

#include "ClueManager.h"
#include "Enemy.h"
#include "Level.h"
#include "LevelLoader.h"
#include "Player.h"

#include "AIHelper.h"
#include "InteractionResult.h"

#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QDir>
#include <QCoreApplication>
#include <QUrl>
#include <QDebug>

namespace {
QString findDifficultyConfigPath()
{
    const QStringList candidates = {
        QDir::current().filePath("levels/difficulty_configs.json"),
        QDir::current().filePath("../levels/difficulty_configs.json"),
        QDir(QCoreApplication::applicationDirPath()).filePath("levels/difficulty_configs.json"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../levels/difficulty_configs.json"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../../levels/difficulty_configs.json")
    };
    for (const QString& path : candidates) {
        if (QFile::exists(path)) {
            return QFileInfo(path).absoluteFilePath();
        }
    }
    return QString();
}
}

Game::Game(QObject* parent)
    : QObject(parent),
      m_state(GameState::START),
      m_difficulty(Difficulty::NORMAL),
      m_score(0),
      m_highScore(0),
      m_timeRemaining(0),
      m_runTimeElapsed(0),
      m_levelTimeElapsed(0),
      m_currentLevelIndex(0),
      m_runSeedBase(0),
      m_runIndex(0),
      m_lootRoomsSpawned(0),
      m_activeGenerationRules(),
      m_difficultyConfig(DifficultyConfig::defaults()),
      m_clueManager(std::make_unique<ClueManager>()),
    m_timer(new QTimer(this)),
    m_aiHelper(std::make_unique<AIHelper>(this))
{
    const QString difficultyConfigPath = findDifficultyConfigPath();
    if (!difficultyConfigPath.isEmpty()) {
        m_difficultyConfig = DifficultyConfig::loadFromJson(difficultyConfigPath);
    }

    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &Game::onTick);
    initSounds();
}

Game::~Game() = default;

void Game::initSounds()
{
    auto setup = [](QSoundEffect& sfx, const QString& path, float volume = 1.0f) {
        sfx.setSource(QUrl::fromLocalFile(path));
        sfx.setVolume(volume);
    };

    const QString base = QCoreApplication::applicationDirPath() + "/assets/sounds/";

    setup(m_sfxCoin,          base + "coin.wav");
    setup(m_sfxWallOpen,      base + "wall_open.wav");
    setup(m_sfxPressurePlate, base + "pressure_plate.wav");
    setup(m_sfxTreasure,      base + "treasure.wav");
    setup(m_sfxWin,           base + "win.wav");
    setup(m_sfxGameOver,      base + "game_over.wav");
    setup(m_sfxBlocked,       base + "blocked.wav");
    setup(m_sfxClue, base + "clue.wav");

    QTimer::singleShot(2000, this, [this]() {
        qDebug() << "blocked status:" << m_sfxBlocked.status();
        qDebug() << "coin status:"    << m_sfxCoin.status();
        qDebug() << "win status:"     << m_sfxWin.status();
    });
}

void Game::setLevelFiles(const QStringList& levelFiles)
{
    m_levelFiles = levelFiles;
}

int Game::levelCount() const
{
    return m_levelFiles.size();
}

void Game::startNewGame(Difficulty diff)
{
    m_score = 0;
    m_runTimeElapsed = 0;
    m_levelTimeElapsed = 0;
    m_runSeedBase = QRandomGenerator::global()->generate();
    ++m_runIndex;
    m_lootRoomsSpawned = 0;
    const DifficultyProfile profile = m_difficultyConfig.selectProfile(diff, m_runSeedBase, m_runIndex);
    m_activeDifficultyProfileId = profile.id;
    m_activeGenerationRules = profile.rules;
    m_timeRemaining = qMax(10, m_activeGenerationRules.startingTime);
    startLevel(0, diff);
}

void Game::startMultiplayerMode()
{
    // TODO: Initialize client connection and transition to multiplayer game state
}

void Game::hostMultiplayerSession(int port)
{
    // TODO: Start server, listen on port, wait for peer to join
}

void Game::joinMultiplayerSession(const QString& host, int port)
{
    // TODO: Connect client to host and sync game state
}

void Game::startLevel(int levelIndex, Difficulty diff)
{
    if (levelIndex < 0) {
        return;
    }
    const bool hasStaticLevel = levelIndex < m_levelFiles.size();

    m_timer->stop();

    m_state = GameState::PLAYING;
    m_difficulty = diff;
    m_currentLevelIndex = levelIndex;

    QJsonArray clues;
    ProceduralGenerationContext generationContext;
    generationContext.rules = m_activeGenerationRules;
    generationContext.runSeedBase = m_runSeedBase;
    generationContext.runIndex = m_runIndex;
    generationContext.levelIndex = levelIndex;
    generationContext.lootRoomsSpawnedThisRun = m_lootRoomsSpawned;
    bool spawnedLootRoom = false;
    if (m_difficulty == Difficulty::EASY) {
        if (hasStaticLevel) {
            m_currentLevel.reset(LevelLoader::loadFromJson(m_levelFiles.at(levelIndex), &clues));
        } else {
            quint32 seed = m_runSeedBase;
            seed ^= static_cast<quint32>(levelIndex + 1) * 0x9e3779b9u;
            seed ^= 0x13579bdfu;
            m_currentLevel.reset(LevelLoader::generateProcedural(static_cast<int>(seed),
                                                                 0,
                                                                 generationContext,
                                                                 &clues,
                                                                 &spawnedLootRoom));
        }
    } else {
        quint32 seed = m_runSeedBase;
        seed ^= static_cast<quint32>(levelIndex + 1) * 0x9e3779b9u;
        seed ^= static_cast<quint32>(m_difficulty == Difficulty::HARD ? 2 : 1) * 0x85ebca6bu;
        const QString anchor = hasStaticLevel ? m_levelFiles.at(levelIndex) : QStringLiteral("procedural");
        for (QChar ch : anchor) {
            seed = seed * 33u + static_cast<quint32>(ch.unicode());
        }

        const int proceduralDifficulty = (m_difficulty == Difficulty::HARD) ? 2 : 1;
        m_currentLevel.reset(LevelLoader::generateProcedural(static_cast<int>(seed),
                                                             proceduralDifficulty,
                                                             generationContext,
                                                             &clues,
                                                             &spawnedLootRoom));

        // Fallback path if procedural generation fails for any reason.
        if (!m_currentLevel && hasStaticLevel) {
            m_currentLevel.reset(LevelLoader::loadFromJson(m_levelFiles.at(levelIndex), &clues));
        }
    }
    if (spawnedLootRoom) {
        ++m_lootRoomsSpawned;
    }
    m_clueManager->loadClues(clues);

    if (!m_currentLevel) {
        m_state = GameState::GAME_OVER;
        emit gameOver(false, m_score);
        return;
    }

    const QPoint spawn = m_currentLevel->getSpawn();
    m_player = std::make_unique<Player>(spawn.x(), spawn.y());
    m_levelTimeElapsed = 0;
    if (m_timeRemaining <= 0) {
        m_timeRemaining = qMax(10, m_activeGenerationRules.startingTime);
    }

    emit levelChanged(m_currentLevelIndex);
    emit timerTick(m_timeRemaining);
    emit gameUpdated();
    m_timer->start();
}

void Game::nextLevel()
{
    if (m_difficulty == Difficulty::EASY) {
        startLevel(m_currentLevelIndex + 1, m_difficulty);
        return;
    }

    if (m_levelFiles.isEmpty()) {
        startLevel(m_currentLevelIndex + 1, m_difficulty);
        return;
    }

    const int nextIndex = (m_currentLevelIndex + 1) % m_levelFiles.size();
    startLevel(nextIndex, m_difficulty);
}

void Game::restartLevel()
{
    startLevel(m_currentLevelIndex, m_difficulty);
}

void Game::saveGame(const QString& filepath)
{
    QJsonObject root;
    root["levelIndex"] = m_currentLevelIndex;
    root["difficulty"] = static_cast<int>(m_difficulty);
    root["score"] = m_score;
    root["highScore"] = m_highScore;
    root["seedBase"] = static_cast<qint64>(m_runSeedBase);
    root["runIndex"] = m_runIndex;
    root["lootRoomsSpawned"] = m_lootRoomsSpawned;
    root["difficultyProfileId"] = m_activeDifficultyProfileId;
    root["runTimeSeconds"] = m_runTimeElapsed;
    root["levelTimeSeconds"] = m_levelTimeElapsed;
    root["timeRemaining"] = m_timeRemaining;

    QJsonDocument doc(root);
    QFile file(filepath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

bool Game::loadGame(const QString& filepath)
{
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return false;
    QJsonObject root = doc.object();

    int levelIndex = root["levelIndex"].toInt(0);
    int diffInt = root["difficulty"].toInt(1);
    m_score = root["score"].toInt(0);
    m_highScore = root["highScore"].toInt(m_score);
    m_runSeedBase = static_cast<quint32>(root["seedBase"].toDouble(0.0));
    m_runIndex = qMax(1, root["runIndex"].toInt(1));
    m_lootRoomsSpawned = qMax(0, root["lootRoomsSpawned"].toInt(0));
    const int savedRunTime = root["runTimeSeconds"].toInt(0);
    const int savedLevelTime = root["levelTimeSeconds"].toInt(0);
    const int savedTimeRemaining = root["timeRemaining"].toInt(-1);
    if (m_runSeedBase == 0u) {
        m_runSeedBase = QRandomGenerator::global()->generate();
    }

    Difficulty diff = Difficulty::NORMAL;
    if (diffInt == 0) diff = Difficulty::EASY;
    else if (diffInt == 2) diff = Difficulty::HARD;

    const DifficultyProfile profile = m_difficultyConfig.selectProfile(diff, m_runSeedBase, m_runIndex);
    m_activeDifficultyProfileId = profile.id;
    m_activeGenerationRules = profile.rules;
    m_timeRemaining = qMax(10, m_activeGenerationRules.startingTime);

    startLevel(levelIndex, diff);
    m_runTimeElapsed = qMax(0, savedRunTime);
    m_levelTimeElapsed = qMax(0, savedLevelTime);
    if (savedTimeRemaining >= 0) {
        m_timeRemaining = qMax(0, savedTimeRemaining);
    } else {
        m_timeRemaining = qMax(0, m_activeGenerationRules.startingTime - m_runTimeElapsed);
    }
    emit timerTick(m_timeRemaining);
    emit gameUpdated();
    return true;
}

bool Game::hasSavedGame(const QString& filepath) const
{
    return QFile::exists(filepath);
}

void Game::handleInput(Direction dir)
{
    if (m_state != GameState::PLAYING || !m_currentLevel || !m_player) {
        return;
    }

    const QPoint currentPos(m_player->getX(), m_player->getY());

    int targetX = currentPos.x();
    int targetY = currentPos.y();
    switch (dir) {
    case Direction::Up:
        targetY -= 1;
        break;
    case Direction::Down:
        targetY += 1;
        break;
    case Direction::Left:
        targetX -= 1;
        break;
    case Direction::Right:
        targetX += 1;
        break;
    case Direction::None:
        return;
    }

    QString blockedReason;
    if (!m_currentLevel->canPlayerEnter(targetX, targetY, *m_player, &blockedReason)) {
        m_sfxBlocked.stop();
        m_sfxBlocked.play();
        if (!blockedReason.isEmpty() && blockedReason.contains("locked", Qt::CaseInsensitive)) {
            emit clueRevealed(blockedReason);
            emit gameUpdated();
        }
        return;
    }

    m_player->move(dir, *m_currentLevel);
    const QPoint newPos(m_player->getX(), m_player->getY());

    if (newPos == currentPos) {
        return;
    }

    for (Enemy* enemy : m_currentLevel->getEnemies()) {
        if (!enemy->isDead() && enemy->getX() == newPos.x() && enemy->getY() == newPos.y()) {
            endRunWithFailure();
            return;
        }
    }

    InteractionResult interaction = m_currentLevel->interactAt(newPos.x(), newPos.y(), *m_player, *m_clueManager);
    if (interaction.scoreDelta != 0) {
        m_score += interaction.scoreDelta;
        if (m_score > m_highScore) {
            m_highScore = m_score;
        }
    }
    if (interaction.coinCollected) {
        m_sfxCoin.stop();
        m_sfxCoin.play();
        emit coinCollected(interaction.coinsCollectedTotal);
    }
    bool clueShownThisStep = false;
    for (const QString& clue : interaction.revealedClues) {
        clueShownThisStep = true;
    m_sfxClue.stop();
    m_sfxClue.play();   
        if (m_aiHelper && m_aiHelper->isEnabled()) {
            m_aiHelper->rephrase(clue, [this](QString transformed) {
                transformed = transformed.trimmed();
                if (!transformed.startsWith("AI Hint:", Qt::CaseInsensitive)) {
                    transformed.prepend("AI Hint: ");
                }
                emit clueRevealed(transformed);
            });
        } else {
            QString fallbackHint = clue.trimmed();
            if (!fallbackHint.startsWith("AI Hint:", Qt::CaseInsensitive)) {
                fallbackHint.prepend("AI Hint: ");
            }
            emit clueRevealed(fallbackHint);
        }
    }
    if (interaction.wallOpened) {
        m_sfxWallOpen.stop();
        m_sfxWallOpen.play();
        emit wallOpened();
    } else if (interaction.triggerActivated && !clueShownThisStep) {
        m_sfxPressurePlate.stop();
        m_sfxPressurePlate.play();
        emit clueRevealed("The pressure plate clicks, but no nearby wall moved.");
    }
    if (interaction.treasureUnlocked && !clueShownThisStep) {
        m_sfxTreasure.play();
        emit treasureUnlocked();
    }
    if (interaction.won) { 
        m_sfxWin.stop();
        m_sfxWin.play();

        if (m_score > m_highScore) {
            m_highScore = m_score;
        }

        if (m_difficulty != Difficulty::EASY) {
            emit clueRevealed("Depth cleared. Descending...");
            nextLevel();
            return;
        }

        m_state = GameState::WIN;
        m_timer->stop();
        emit gameUpdated();
        emit gameOver(true, m_score);
        return;
    }

    emit gameUpdated();
}

void Game::pause()
{
    if (m_state != GameState::PLAYING) {
        return;
    }
    m_state = GameState::PAUSED;
    m_timer->stop();
    emit gameUpdated();
}

void Game::resume()
{
    if (m_state != GameState::PAUSED) {
        return;
    }
    m_state = GameState::PLAYING;
    m_timer->start();
    emit gameUpdated();
}

GameState Game::state() const
{
    return m_state;
}

Difficulty Game::difficulty() const
{
    return m_difficulty;
}

int Game::score() const
{
    return m_score;
}

int Game::highScore() const
{
    return m_highScore;
}

int Game::timeRemaining() const
{
    return m_timeRemaining;
}

int Game::runTimeSeconds() const
{
    return m_runTimeElapsed;
}

int Game::levelTimeSeconds() const
{
    return m_levelTimeElapsed;
}

int Game::currentLevelIndex() const
{
    return m_currentLevelIndex;
}

QString Game::currentLevelName() const
{
    return m_currentLevel ? m_currentLevel->getName() : QString();
}

int Game::coinsCollected() const
{
    return m_player ? m_player->getCoinsCollected() : 0;
}

QPoint Game::playerPosition() const
{
    return m_player ? QPoint(m_player->getX(), m_player->getY()) : QPoint();
}

const Level* Game::level() const
{
    return m_currentLevel.get();
}

const Player* Game::player() const
{
    return m_player.get();
}

void Game::onTick()
{
    if (m_state != GameState::PLAYING) {
        return;
    }

    if (m_currentLevel) {
        for (Enemy* enemy : m_currentLevel->getEnemies()) {
            enemy->advanceAnimation();
            enemy->update(*m_currentLevel, *m_player);

            if (!enemy->isDead() && m_player && enemy->getX() == m_player->getX() && enemy->getY() == m_player->getY()) {
                endRunWithFailure();
                return;
            }
        }
    }

    if (m_timeRemaining > 0) {
        m_timeRemaining -= 1;
    }
    if (m_timeRemaining < 0) {
        m_timeRemaining = 0;
    }

    m_runTimeElapsed += 1;
    m_levelTimeElapsed += 1;

    emit timerTick(m_timeRemaining);

    if (m_timeRemaining == 0) {
        endRunWithFailure();
        return;
    }

    emit gameUpdated();
}

void Game::endRunWithFailure()
{
    m_sfxGameOver.stop(); 
    m_sfxGameOver.play();
    m_state = GameState::GAME_OVER;
    m_timer->stop();

    const int finalScore = m_score;
    if (finalScore > m_highScore) {
        m_highScore = finalScore;
    }

    emit gameUpdated();
    emit gameOver(false, finalScore);

    m_score = 0;
}