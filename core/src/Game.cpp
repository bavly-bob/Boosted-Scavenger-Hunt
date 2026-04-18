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
#include <QJsonDocument>
#include <QJsonObject>

namespace {
int difficultyTimeOffsetSeconds(Difficulty difficulty)
{
    switch (difficulty) {
    case Difficulty::EASY:
        return 30;
    case Difficulty::NORMAL:
        return 0;
    case Difficulty::HARD:
        return -15;
    }
    return 0;
}
} // namespace

Game::Game(QObject* parent)
    : QObject(parent),
      m_state(GameState::START),
      m_difficulty(Difficulty::NORMAL),
      m_score(0),
      m_timeRemaining(0),
      m_currentLevelIndex(0),
      m_clueManager(std::make_unique<ClueManager>()),
    m_timer(new QTimer(this)),
    m_aiHelper(std::make_unique<AIHelper>(this))
{
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &Game::onTick);
}

Game::~Game() = default;

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
    startLevel(0, diff);
}

void Game::startLevel(int levelIndex, Difficulty diff)
{
    if (levelIndex < 0 || levelIndex >= m_levelFiles.size()) {
        return;
    }

    m_timer->stop();

    m_state = GameState::PLAYING;
    m_difficulty = diff;
    m_currentLevelIndex = levelIndex;

    QJsonArray clues;
    m_currentLevel.reset(LevelLoader::loadFromJson(m_levelFiles.at(levelIndex), &clues));
    m_clueManager->loadClues(clues);

    if (!m_currentLevel) {
        m_state = GameState::GAME_OVER;
        emit gameOver(false, m_score);
        return;
    }

    const QPoint spawn = m_currentLevel->getSpawn();
    m_player = std::make_unique<Player>(spawn.x(), spawn.y());

    const int baseTime = m_currentLevel->getTimeLimit();
    m_timeRemaining = baseTime + difficultyTimeOffsetSeconds(m_difficulty);
    if (m_timeRemaining < 10) {
        m_timeRemaining = 10;
    }

    emit timerTick(m_timeRemaining);
    emit gameUpdated();
    m_timer->start();
}

void Game::nextLevel()
{
    if (m_currentLevelIndex + 1 >= m_levelFiles.size()) {
        return;
    }
    startLevel(m_currentLevelIndex + 1, m_difficulty);
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
    
    Difficulty diff = Difficulty::NORMAL;
    if (diffInt == 0) diff = Difficulty::EASY;
    else if (diffInt == 2) diff = Difficulty::HARD;
    
    startLevel(levelIndex, diff);
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

    // Check collision with enemies after player moves
    for (Enemy* enemy : m_currentLevel->getEnemies()) {
        if (!enemy->isDead() && enemy->getX() == newPos.x() && enemy->getY() == newPos.y()) {
            m_state = GameState::GAME_OVER;
            m_timer->stop();
            emit gameUpdated();
            emit gameOver(false, m_score);
            return;
        }
    }

    InteractionResult interaction = m_currentLevel->interactAt(newPos.x(), newPos.y(), *m_player, *m_clueManager);
    if (interaction.scoreDelta != 0) {
        m_score += interaction.scoreDelta;
    }
    if (interaction.coinCollected) {
        emit coinCollected(interaction.coinsCollectedTotal);
    }
    bool clueShownThisStep = false;
    for (const QString& clue : interaction.revealedClues) {
        clueShownThisStep = true;
        if (m_aiHelper && m_aiHelper->isEnabled()) {
            m_aiHelper->rephrase(clue, [this](QString transformed) {
                transformed = transformed.trimmed();
                if (!transformed.startsWith("AI Hint:", Qt::CaseInsensitive)) {
                    transformed.prepend("AI Hint: ");
                }
                emit clueRevealed(transformed);
            });
        } else {
            emit clueRevealed(clue);
        }
    }
    if (interaction.wallOpened) {
        emit wallOpened();
    } else if (interaction.triggerActivated && !clueShownThisStep) {
        emit clueRevealed("The pressure plate clicks, but no nearby wall moved.");
    }
    if (interaction.treasureUnlocked && !clueShownThisStep) {
        emit treasureUnlocked();
    }
    if (interaction.won) {
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

int Game::timeRemaining() const
{
    return m_timeRemaining;
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

    // Advance enemy logic (enemy animation is also ticked by the render timer)
    if (m_currentLevel) {
        for (Enemy* enemy : m_currentLevel->getEnemies()) {
            enemy->advanceAnimation();
            enemy->update(*m_currentLevel, *m_player);
            
            // Check collision with player after enemy moves
            if (!enemy->isDead() && m_player && enemy->getX() == m_player->getX() && enemy->getY() == m_player->getY()) {
                m_state = GameState::GAME_OVER;
                m_timer->stop();
                emit gameUpdated();
                emit gameOver(false, m_score);
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

    emit timerTick(m_timeRemaining);

    if (m_timeRemaining == 0) {
        m_state = GameState::GAME_OVER;
        m_timer->stop();
        emit gameUpdated();
        emit gameOver(false, m_score);
        return;
    }

    emit gameUpdated();
}
