#include "Game.h"

#include "ClueManager.h"
#include "Enemy.h"
#include "Level.h"
#include "LevelLoader.h"
#include "Player.h"
#include "TreasureRoom.h"

#include "AIHelper.h"
#include "InteractionResult.h"
#include "protocol.hpp"

#include <boost/asio.hpp>

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

#include <chrono>
#include <thread>

namespace {
using boost::asio::ip::tcp;
constexpr std::uint32_t kMaxFrameSize = 10u * 1024u * 1024u;

bool readFramedJson(tcp::socket& socket, QJsonObject& out, QString& error)
{
    std::array<char, 4> header{};
    boost::system::error_code ec;
    boost::asio::read(socket, boost::asio::buffer(header), ec);
    if (ec) {
        error = QStringLiteral("Receive failed: %1").arg(QString::fromStdString(ec.message()));
        return false;
    }

    const std::uint32_t length = netwatch::protocol::decodeHeader(header.data());
    if (length == 0 || length > kMaxFrameSize) {
        error = QStringLiteral("Invalid message size from peer.");
        return false;
    }

    std::string payload(length, '\0');
    boost::asio::read(socket, boost::asio::buffer(payload), ec);
    if (ec) {
        error = QStringLiteral("Receive failed: %1").arg(QString::fromStdString(ec.message()));
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(payload.data(), static_cast<int>(payload.size())), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        error = QStringLiteral("Received invalid JSON payload.");
        return false;
    }

    out = doc.object();
    return true;
}

bool writeFramedJson(tcp::socket& socket, const QJsonObject& obj, QString& error)
{
    const std::string payload = QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
    const std::string frame = netwatch::protocol::encode(payload);

    boost::system::error_code ec;
    boost::asio::write(socket, boost::asio::buffer(frame), ec);
    if (ec) {
        error = QStringLiteral("Send failed: %1").arg(QString::fromStdString(ec.message()));
        return false;
    }

    return true;
}

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

QPoint findSecondarySpawn(const Level& level, const QPoint& primary)
{
    static const std::array<QPoint, 8> offsets = {
        QPoint(1, 0), QPoint(-1, 0), QPoint(0, 1), QPoint(0, -1),
        QPoint(1, 1), QPoint(-1, 1), QPoint(1, -1), QPoint(-1, -1)
    };

    for (const QPoint& offset : offsets) {
        const QPoint candidate = primary + offset;
        if (level.isInBounds(candidate.x(), candidate.y()) && level.isWalkable(candidate.x(), candidate.y())) {
            return candidate;
        }
    }

    for (int y = 0; y < level.getHeight(); ++y) {
        for (int x = 0; x < level.getWidth(); ++x) {
            if (x == primary.x() && y == primary.y()) {
                continue;
            }
            if (level.isWalkable(x, y)) {
                return QPoint(x, y);
            }
        }
    }

    return primary;
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
      m_multiplayerMode(false),
      m_playerReachedTreasure{false, false},
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
    m_multiplayerMode = false;
    resetTreasureReachState();
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
    m_multiplayerMode = true;
    resetTreasureReachState();
    m_score = 0;
    m_runTimeElapsed = 0;
    m_levelTimeElapsed = 0;
    m_runSeedBase = QRandomGenerator::global()->generate();
    ++m_runIndex;
    m_lootRoomsSpawned = 0;
    const DifficultyProfile profile = m_difficultyConfig.selectProfile(Difficulty::HARD, m_runSeedBase, m_runIndex);
    m_activeDifficultyProfileId = profile.id;
    m_activeGenerationRules = profile.rules;
    m_timeRemaining = qMax(10, m_activeGenerationRules.startingTime);
    startLevel(0, Difficulty::HARD);
    emit clueRevealed("Multiplayer co-op: hard mode, shared coins, reach treasure together.");
}

void Game::hostMultiplayerSession(int port)
{
    if (port <= 0 || port > 65535) {
        emit clueRevealed("Invalid port.");
        return;
    }

    boost::asio::io_context io;
    boost::system::error_code ec;

    tcp::acceptor acceptor(io);
    acceptor.open(tcp::v4(), ec);
    if (ec) {
        emit clueRevealed(QStringLiteral("Host failed: %1").arg(QString::fromStdString(ec.message())));
        return;
    }
    acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec) {
        emit clueRevealed(QStringLiteral("Host failed: %1").arg(QString::fromStdString(ec.message())));
        return;
    }
    acceptor.bind(tcp::endpoint(tcp::v4(), static_cast<unsigned short>(port)), ec);
    if (ec) {
        emit clueRevealed(QStringLiteral("Host failed on port %1: %2").arg(port).arg(QString::fromStdString(ec.message())));
        return;
    }
    acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        emit clueRevealed(QStringLiteral("Host failed: %1").arg(QString::fromStdString(ec.message())));
        return;
    }

    emit clueRevealed(QStringLiteral("Hosting on port %1. Waiting for peer...").arg(port));

    tcp::socket socket(io);
    acceptor.accept(socket, ec);
    if (ec) {
        emit clueRevealed(QStringLiteral("Accept failed: %1").arg(QString::fromStdString(ec.message())));
        return;
    }

    QJsonObject joinRequest;
    QString errorText;
    if (!readFramedJson(socket, joinRequest, errorText)) {
        emit clueRevealed(errorText);
        return;
    }

    if (joinRequest.value("type").toString() != QStringLiteral("join")) {
        emit clueRevealed("Peer sent invalid join request.");
        return;
    }

    const QJsonObject state{
        {QStringLiteral("levelIndex"), m_currentLevelIndex},
        {QStringLiteral("difficulty"), static_cast<int>(m_difficulty)},
        {QStringLiteral("score"), m_score},
        {QStringLiteral("highScore"), m_highScore},
        {QStringLiteral("seedBase"), static_cast<qint64>(m_runSeedBase)},
        {QStringLiteral("runIndex"), m_runIndex},
        {QStringLiteral("lootRoomsSpawned"), m_lootRoomsSpawned},
        {QStringLiteral("runTimeSeconds"), m_runTimeElapsed},
        {QStringLiteral("levelTimeSeconds"), m_levelTimeElapsed},
        {QStringLiteral("timeRemaining"), m_timeRemaining}
    };

    if (!writeFramedJson(socket, state, errorText)) {
        emit clueRevealed(errorText);
        return;
    }

    emit clueRevealed(QStringLiteral("Peer joined from %1").arg(QString::fromStdString(socket.remote_endpoint().address().to_string())));
}

void Game::joinMultiplayerSession(const QString& host, int port)
{
    if (host.trimmed().isEmpty() || port <= 0 || port > 65535) {
        emit clueRevealed("Invalid host or port.");
        return;
    }

    boost::asio::io_context io;
    boost::system::error_code ec;

    tcp::resolver resolver(io);
    const auto endpoints = resolver.resolve(host.trimmed().toStdString(), std::to_string(port), ec);
    if (ec) {
        emit clueRevealed(QStringLiteral("Resolve failed for %1:%2 (%3)").arg(host.trimmed()).arg(port).arg(QString::fromStdString(ec.message())));
        return;
    }

    emit clueRevealed(QStringLiteral("Joining %1 on room port %2. Waiting for host...").arg(host.trimmed()).arg(port));

    tcp::socket socket(io);
    bool connected = false;
    boost::system::error_code lastConnectError;
    for (int attempt = 0; attempt < 60; ++attempt) {
        socket.close();
        socket = tcp::socket(io);
        boost::asio::connect(socket, endpoints, ec);
        if (!ec) {
            connected = true;
            break;
        }
        lastConnectError = ec;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    if (!connected) {
        emit clueRevealed(QStringLiteral("Failed to connect to %1:%2 (%3)")
                              .arg(host.trimmed())
                              .arg(port)
                              .arg(QString::fromStdString(lastConnectError.message())));
        return;
    }

    const QJsonObject joinRequest{
        {QStringLiteral("type"), QStringLiteral("join")},
        {QStringLiteral("version"), 1}
    };

    QString errorText;
    if (!writeFramedJson(socket, joinRequest, errorText)) {
        emit clueRevealed(errorText);
        return;
    }

    QJsonObject root;
    if (!readFramedJson(socket, root, errorText)) {
        emit clueRevealed(errorText);
        return;
    }

    const int levelIndex = qMax(0, root.value("levelIndex").toInt(0));
    const int diffInt = root.value("difficulty").toInt(static_cast<int>(Difficulty::HARD));

    Difficulty diff = Difficulty::NORMAL;
    if (diffInt == 0) {
        diff = Difficulty::EASY;
    } else if (diffInt == 2) {
        diff = Difficulty::HARD;
    }

    m_multiplayerMode = true;
    resetTreasureReachState();
    m_score = root.value("score").toInt(0);
    m_highScore = root.value("highScore").toInt(m_score);
    m_runSeedBase = static_cast<quint32>(root.value("seedBase").toDouble(0.0));
    m_runIndex = qMax(1, root.value("runIndex").toInt(1));
    m_lootRoomsSpawned = qMax(0, root.value("lootRoomsSpawned").toInt(0));
    if (m_runSeedBase == 0u) {
        m_runSeedBase = QRandomGenerator::global()->generate();
    }

    const DifficultyProfile profile = m_difficultyConfig.selectProfile(diff, m_runSeedBase, m_runIndex);
    m_activeDifficultyProfileId = profile.id;
    m_activeGenerationRules = profile.rules;
    m_timeRemaining = qMax(10, m_activeGenerationRules.startingTime);

    startLevel(levelIndex, diff);
    m_runTimeElapsed = qMax(0, root.value("runTimeSeconds").toInt(0));
    m_levelTimeElapsed = qMax(0, root.value("levelTimeSeconds").toInt(0));
    m_timeRemaining = qMax(0, root.value("timeRemaining").toInt(m_timeRemaining));

    emit clueRevealed(QStringLiteral("Connected to host %1:%2").arg(host.trimmed()).arg(port));
    emit timerTick(m_timeRemaining);
    emit gameUpdated();
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
    m_players[0] = std::make_unique<Player>(spawn.x(), spawn.y());
    m_players[1].reset();
    if (m_multiplayerMode) {
        const QPoint secondarySpawn = findSecondarySpawn(*m_currentLevel, spawn);
        m_players[1] = std::make_unique<Player>(secondarySpawn.x(), secondarySpawn.y());
    }
    resetTreasureReachState();
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
    m_multiplayerMode = false;
    resetTreasureReachState();
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
    handleInputForPlayer(0, dir);
}

void Game::handleInputForPlayer(int playerIndex, Direction dir)
{
    if (m_state != GameState::PLAYING || !m_currentLevel || dir == Direction::None) {
        return;
    }
    if (playerIndex < 0 || playerIndex >= static_cast<int>(m_players.size())) {
        return;
    }

    Player* activePlayer = m_players[playerIndex].get();
    if (!activePlayer) {
        return;
    }
    if (m_multiplayerMode && m_playerReachedTreasure[playerIndex]) {
        return;
    }

    const QPoint currentPos(activePlayer->getX(), activePlayer->getY());

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
        break;
    }

    QString blockedReason;
    if (!m_currentLevel->canPlayerEnter(targetX, targetY, *activePlayer, &blockedReason)) {
        m_sfxBlocked.stop();
        m_sfxBlocked.play();
        if (!blockedReason.isEmpty() && blockedReason.contains("locked", Qt::CaseInsensitive)) {
            emit clueRevealed(blockedReason);
            emit gameUpdated();
        }
        return;
    }
    if (isCellOccupiedByOtherPlayer(playerIndex, targetX, targetY)) {
        m_sfxBlocked.stop();
        m_sfxBlocked.play();
        return;
    }

    activePlayer->move(dir, *m_currentLevel);
    const QPoint newPos(activePlayer->getX(), activePlayer->getY());

    if (newPos == currentPos) {
        return;
    }

    for (Enemy* enemy : m_currentLevel->getEnemies()) {
        if (!enemy->isDead() && enemy->getX() == newPos.x() && enemy->getY() == newPos.y()) {
            endRunWithFailure();
            return;
        }
    }

    InteractionResult interaction = m_currentLevel->interactAt(newPos.x(), newPos.y(), *activePlayer, *m_clueManager);
    if (interaction.scoreDelta != 0) {
        m_score += interaction.scoreDelta;
        if (m_score > m_highScore) {
            m_highScore = m_score;
        }
    }
    if (interaction.coinCollected) {
        if (m_multiplayerMode) {
            const int sharedCoins = activePlayer->getCoinsCollected();
            for (auto& maybePlayer : m_players) {
                if (!maybePlayer) {
                    continue;
                }
                while (maybePlayer->getCoinsCollected() < sharedCoins) {
                    maybePlayer->collectCoin();
                }
            }
            emit coinCollected(sharedCoins);
        } else {
            emit coinCollected(interaction.coinsCollectedTotal);
        }
        m_sfxCoin.stop();
        m_sfxCoin.play();
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
        handlePlayerReachedTreasure(playerIndex);
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
    int sharedCoins = 0;
    for (const auto& maybePlayer : m_players) {
        if (maybePlayer) {
            sharedCoins = qMax(sharedCoins, maybePlayer->getCoinsCollected());
        }
    }
    return sharedCoins;
}

QPoint Game::playerPosition(int playerIndex) const
{
    const Player* p = player(playerIndex);
    return p ? QPoint(p->getX(), p->getY()) : QPoint();
}

int Game::playerCount() const
{
    return m_players[1] ? 2 : (m_players[0] ? 1 : 0);
}

bool Game::isMultiplayerMode() const
{
    return m_multiplayerMode;
}

const Level* Game::level() const
{
    return m_currentLevel.get();
}

const Player* Game::player(int playerIndex) const
{
    if (playerIndex < 0 || playerIndex >= static_cast<int>(m_players.size())) {
        return nullptr;
    }
    return m_players[playerIndex].get();
}

void Game::onTick()
{
    if (m_state != GameState::PLAYING) {
        return;
    }

    if (m_currentLevel) {
        const Player* chaseTarget = m_players[0].get();
        for (Enemy* enemy : m_currentLevel->getEnemies()) {
            enemy->advanceAnimation();
            const Player* target = chaseTarget;
            if (m_multiplayerMode && m_players[0] && m_players[1]) {
                const int d0 = qAbs(enemy->getX() - m_players[0]->getX()) + qAbs(enemy->getY() - m_players[0]->getY());
                const int d1 = qAbs(enemy->getX() - m_players[1]->getX()) + qAbs(enemy->getY() - m_players[1]->getY());
                target = (d1 < d0) ? static_cast<const Player*>(m_players[1].get())
                                   : static_cast<const Player*>(m_players[0].get());
            }
            if (target) {
                enemy->update(*m_currentLevel, *target);
            }

            if (!enemy->isDead()) {
                for (const auto& maybePlayer : m_players) {
                    if (!maybePlayer) {
                        continue;
                    }
                    if (enemy->getX() == maybePlayer->getX() && enemy->getY() == maybePlayer->getY()) {
                        endRunWithFailure();
                        return;
                    }
                }
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

void Game::resetTreasureReachState()
{
    m_playerReachedTreasure = {false, false};
}

bool Game::isCellOccupiedByOtherPlayer(int playerIndex, int x, int y) const
{
    if (!m_multiplayerMode) {
        return false;
    }

    for (int i = 0; i < static_cast<int>(m_players.size()); ++i) {
        if (i == playerIndex || !m_players[i]) {
            continue;
        }
        if (m_players[i]->getX() == x && m_players[i]->getY() == y) {
            const TreasureRoom* treasure = m_currentLevel ? m_currentLevel->getTreasureRoom() : nullptr;
            if (treasure && treasure->getX() == x && treasure->getY() == y) {
                return false;
            }
            return true;
        }
    }
    return false;
}

void Game::handlePlayerReachedTreasure(int playerIndex)
{
    if (!m_multiplayerMode) {
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

    m_playerReachedTreasure[playerIndex] = true;
    const bool allReady = m_playerReachedTreasure[0] && m_playerReachedTreasure[1];
    if (!allReady) {
        emit waitingForTeammate(playerIndex);
        emit clueRevealed("You reached the treasure. Wait for your teammate.");
        emit gameUpdated();
        return;
    }

    m_sfxWin.stop();
    m_sfxWin.play();
    if (m_score > m_highScore) {
        m_highScore = m_score;
    }
    emit clueRevealed("Both hunters reached the treasure. Descending...");
    nextLevel();
}
