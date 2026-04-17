#include "GameWindow.h"

#include "Game.h"
#include "GameOverOverlay.h"
#include "GameObject.h"
#include "InputHandler.h"
#include "Level.h"
#include "LevelLoader.h"
#include "PauseOverlay.h"
#include "Player.h"

#include <QCoreApplication>
#include <QDir>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QPainter>

namespace {
QString formatTime(int seconds)
{
    const int m = seconds / 60;
    const int s = seconds % 60;
    return QString("%1:%2")
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

QString findLevelsDir()
{
    const QStringList candidates = {
        QDir::current().filePath("levels"),
        QDir::current().filePath("../levels"),
        QDir(QCoreApplication::applicationDirPath()).filePath("levels"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../levels"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../../levels"),
    };

    for (const QString& candidate : candidates) {
        QDir d(candidate);
        if (d.exists()) {
            return d.absolutePath();
        }
    }

    return QDir::current().absolutePath();
}
} // namespace

GameWindow::GameWindow(QWidget *parent)
    : QWidget(parent),
      m_game(new Game(this)),
      m_pauseOverlay(new PauseOverlay(this)),
      m_gameOverOverlay(new GameOverOverlay(this)),
      m_levelsConfigured(false)
{
    setWindowTitle("Scavenger Hunt");
    setFocusPolicy(Qt::StrongFocus);

    m_pauseOverlay->hide();
    m_gameOverOverlay->hide();

    connect(m_game, &Game::gameUpdated, this, QOverload<>::of(&GameWindow::update));
    connect(m_game, &Game::clueRevealed, this, &GameWindow::onClueRevealed);
    connect(m_game, &Game::wallOpened, this, &GameWindow::onWallOpened);
    connect(m_game, &Game::treasureUnlocked, this, &GameWindow::onTreasureUnlocked);
    connect(m_game, &Game::gameOver, this, &GameWindow::onGameOver);

    connect(m_pauseOverlay, &PauseOverlay::resumeRequested, this, [this]() {
        hidePauseOverlay();
        m_game->resume();
        setFocus();
    });
    connect(m_pauseOverlay, &PauseOverlay::restartRequested, this, [this]() {
        hidePauseOverlay();
        m_game->restartLevel();
        resizeToCurrentLevel();
        setFocus();
    });
    connect(m_pauseOverlay, &PauseOverlay::mainMenuRequested, this, [this]() {
        hidePauseOverlay();
        m_game->pause();
        m_gameOverOverlay->hide();
        hide();
        emit quitToMainMenuRequested();
    });

    connect(m_gameOverOverlay, &GameOverOverlay::nextLevelRequested, this, [this]() {
        m_gameOverOverlay->hide();
        m_game->nextLevel();
        resizeToCurrentLevel();
        setFocus();
    });
    connect(m_gameOverOverlay, &GameOverOverlay::restartRequested, this, [this]() {
        m_gameOverOverlay->hide();
        m_game->restartLevel();
        resizeToCurrentLevel();
        setFocus();
    });
    connect(m_gameOverOverlay, &GameOverOverlay::mainMenuRequested, this, [this]() {
        m_gameOverOverlay->hide();
        m_game->pause();
        hide();
        emit quitToMainMenuRequested();
    });

    setFixedSize(600, 520);
}

void GameWindow::startNewGame(Difficulty difficulty)
{
    ensureLevelsConfigured();
    m_statusText.clear();
    m_pauseOverlay->hide();
    m_gameOverOverlay->hide();

    m_game->startNewGame(difficulty);
    resizeToCurrentLevel();

    show();
    setFocus();
}

void GameWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);

    QLinearGradient grad(0, 0, width(), height());
    grad.setColorAt(0.0, QColor(25, 10, 65));
    grad.setColorAt(0.4, QColor(15, 50, 70));
    grad.setColorAt(1.0, QColor(10, 20, 35));
    p.fillRect(rect(), grad);

    // HUD
    const QRect hud(0, 0, width(), HUD_HEIGHT);
    p.fillRect(hud, QColor(0, 0, 0, 120));

    const QString levelName = m_game->currentLevelName();
    const QString scoreText = QString("Score: %1").arg(m_game->score());
    const QString coinsText = QString("Coins: %1/3").arg(m_game->coinsCollected());
    const QString timeText = QString("Time: %1").arg(formatTime(m_game->timeRemaining()));

    p.setPen(QColor(240, 235, 225));
    QFont titleFont = p.font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    p.setFont(titleFont);
    p.drawText(QRect(12, 8, width() - 24, 24), Qt::AlignLeft | Qt::AlignVCenter, levelName.isEmpty() ? "Scavenger Hunt" : levelName);

    QFont infoFont = p.font();
    infoFont.setBold(false);
    infoFont.setPointSize(infoFont.pointSize() - 1);
    p.setFont(infoFont);
    p.setPen(QColor(220, 215, 205));
    p.drawText(QRect(12, 34, width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter, scoreText);
    p.drawText(QRect(width() / 3, 34, width() / 3, 18), Qt::AlignHCenter | Qt::AlignVCenter, coinsText);
    p.drawText(QRect(width() - 140, 34, 128, 18), Qt::AlignRight | Qt::AlignVCenter, timeText);

    if (!m_statusText.isEmpty()) {
        p.setPen(QColor(230, 200, 120));
        p.drawText(QRect(12, 52, width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter, m_statusText);
    }

    const Level* level = m_game->level();
    const Player* player = m_game->player();
    if (!level || !player) {
        return;
    }

    // Grid
    p.save();
    p.translate(0, HUD_HEIGHT);
    for (int y = 0; y < level->getHeight(); ++y) {
        for (int x = 0; x < level->getWidth(); ++x) {
            const QRect cell(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
            const QColor base = ((x + y) % 2 == 0) ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 18);
            p.fillRect(cell, base);
            p.setPen(QPen(QColor(0, 0, 0, 60), 1));
            p.drawRect(cell);
        }
    }

    for (GameObject* obj : level->getObjects()) {
        if (obj) {
            obj->draw(p, CELL_SIZE);
        }
    }
    player->draw(p, CELL_SIZE);
    p.restore();
}

void GameWindow::keyPressEvent(QKeyEvent *event)
{
    if (!event) {
        return;
    }

    if (InputHandler::isPauseKey(event->key())) {
        if (m_game->state() == GameState::PLAYING) {
            showPauseOverlay();
            m_game->pause();
        } else if (m_game->state() == GameState::PAUSED) {
            hidePauseOverlay();
            m_game->resume();
        }
        return;
    }

    if (m_game->state() != GameState::PLAYING) {
        QWidget::keyPressEvent(event);
        return;
    }

    const Direction dir = InputHandler::keyToDirection(event->key());
    if (dir != Direction::None) {
        m_game->handleInput(dir);
        return;
    }

    QWidget::keyPressEvent(event);
}

void GameWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_pauseOverlay->setGeometry(rect());
    m_gameOverOverlay->setGeometry(rect());
}

void GameWindow::onClueRevealed(const QString& text)
{
    m_statusText = text;
    update();
}

void GameWindow::onWallOpened()
{
    m_statusText = "You hear stone grinding... a passage opens!";
    update();
}

void GameWindow::onTreasureUnlocked()
{
    m_statusText = "Treasure unlocked! Head to the treasure room!";
    update();
}

void GameWindow::onGameOver(bool won, int score)
{
    hidePauseOverlay();

    const bool hasNext = won && (m_game->currentLevelIndex() + 1 < m_game->levelCount());
    m_gameOverOverlay->setResult(won, score, hasNext);
    m_gameOverOverlay->setGeometry(rect());
    m_gameOverOverlay->show();
    m_gameOverOverlay->raise();
}

void GameWindow::ensureLevelsConfigured()
{
    if (m_levelsConfigured) {
        return;
    }

    const QString levelsDir = findLevelsDir();
    const QStringList levelFiles = LevelLoader::getAvailableLevels(levelsDir);
    m_game->setLevelFiles(levelFiles);
    m_levelsConfigured = true;
}

void GameWindow::resizeToCurrentLevel()
{
    const Level* level = m_game->level();
    if (!level || level->getWidth() <= 0 || level->getHeight() <= 0) {
        return;
    }

    setFixedSize(level->getWidth() * CELL_SIZE, HUD_HEIGHT + level->getHeight() * CELL_SIZE);
    m_pauseOverlay->setGeometry(rect());
    m_gameOverOverlay->setGeometry(rect());
}

void GameWindow::showPauseOverlay()
{
    m_pauseOverlay->setGeometry(rect());
    m_pauseOverlay->show();
    m_pauseOverlay->raise();
}

void GameWindow::hidePauseOverlay()
{
    m_pauseOverlay->hide();
}
