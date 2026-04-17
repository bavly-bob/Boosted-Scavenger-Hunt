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
#include <QTimer>

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
      m_renderTimer(new QTimer(this)),
      m_cameraPx(0.0, 0.0),
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

    // Drive smooth camera animation even when no inputs occur.
    m_renderTimer->setInterval(16);
    connect(m_renderTimer, &QTimer::timeout, this, QOverload<>::of(&GameWindow::update));
    m_renderTimer->start();

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

    setFixedSize(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
}

void GameWindow::startNewGame(Difficulty difficulty)
{
    ensureLevelsConfigured();
    m_statusText.clear();
    m_pauseOverlay->hide();
    m_gameOverOverlay->hide();

    m_game->startNewGame(difficulty);
    resizeToCurrentLevel();
    snapCameraToPlayer();

    if (const Level* level = m_game->level()) {
        m_explored = QVector<QVector<bool>>(level->getHeight(), QVector<bool>(level->getWidth(), false));
    } else {
        m_explored.clear();
    }

    show();
    setFocus();
}

void GameWindow::snapCameraToPlayer()
{
    const Level* level = m_game->level();
    const Player* player = m_game->player();
    if (!level || !player) {
        m_cameraPx = QPointF(0.0, 0.0);
        return;
    }

    const int mapPixelW = level->getWidth() * TILE_SIZE;
    const int mapPixelH = level->getHeight() * TILE_SIZE;

    const int viewPixelW = width();
    const int viewPixelH = height() - HUD_HEIGHT;

    const qreal playerPxX = player->getX() * TILE_SIZE + TILE_SIZE / 2.0;
    const qreal playerPxY = player->getY() * TILE_SIZE + TILE_SIZE / 2.0;

    qreal camX = playerPxX - viewPixelW / 2.0;
    qreal camY = playerPxY - viewPixelH / 2.0;

    camX = qBound<qreal>(0.0, camX, qMax<qreal>(0.0, mapPixelW - viewPixelW));
    camY = qBound<qreal>(0.0, camY, qMax<qreal>(0.0, mapPixelH - viewPixelH));
    m_cameraPx = QPointF(camX, camY);
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

    p.save();
    p.translate(0, HUD_HEIGHT);
    // Camera + tile culling (render only visible tiles).
    const int mapPixelW = level->getWidth() * TILE_SIZE;
    const int mapPixelH = level->getHeight() * TILE_SIZE;

    const int viewPixelW = width();
    const int viewPixelH = height() - HUD_HEIGHT;

    const qreal playerPxX = player->getX() * TILE_SIZE + TILE_SIZE / 2.0;
    const qreal playerPxY = player->getY() * TILE_SIZE + TILE_SIZE / 2.0;

    qreal targetCamX = playerPxX - viewPixelW / 2.0;
    qreal targetCamY = playerPxY - viewPixelH / 2.0;
    targetCamX = qBound<qreal>(0.0, targetCamX, qMax<qreal>(0.0, mapPixelW - viewPixelW));
    targetCamY = qBound<qreal>(0.0, targetCamY, qMax<qreal>(0.0, mapPixelH - viewPixelH));

    m_cameraPx.setX(m_cameraPx.x() + (targetCamX - m_cameraPx.x()) * CAMERA_LERP);
    m_cameraPx.setY(m_cameraPx.y() + (targetCamY - m_cameraPx.y()) * CAMERA_LERP);
    const int camX = static_cast<int>(m_cameraPx.x());
    const int camY = static_cast<int>(m_cameraPx.y());

    // Convert camera pixels -> tile bounds.
    const int startX = qMax(0, camX / TILE_SIZE);
    const int startY = qMax(0, camY / TILE_SIZE);
    const int endX = qMin(level->getWidth() - 1, (camX + viewPixelW + TILE_SIZE - 1) / TILE_SIZE);
    const int endY = qMin(level->getHeight() - 1, (camY + viewPixelH + TILE_SIZE - 1) / TILE_SIZE);

    // Draw base tiles only inside visible bounds.
    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            const int screenX = x * TILE_SIZE - camX;
            const int screenY = y * TILE_SIZE - camY;
            const QRect cell(screenX, screenY, TILE_SIZE, TILE_SIZE);
            const int tileId = level->tileAt(x, y);
            QColor base;
            switch (static_cast<CellType>(tileId)) {
            case CellType::Wall:
                base = QColor(65, 65, 75, 210);
                break;
            case CellType::HiddenWall:
                base = QColor(75, 75, 90, 210);
                break;
            default:
                base = ((x + y) % 2 == 0) ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 18);
                break;
            }
            p.fillRect(cell, base);
            p.setPen(QPen(QColor(0, 0, 0, 60), 1));
            p.drawRect(cell);
        }
    }

    for (GameObject* obj : level->getObjects()) {
        if (obj) {
            const int ox = obj->getX();
            const int oy = obj->getY();
            if (ox >= startX && ox <= endX && oy >= startY && oy <= endY) {
                p.save();
                p.translate(-camX, -camY);
                obj->draw(p, TILE_SIZE);
                p.restore();
            }
        }
    }
    p.save();
    p.translate(-camX, -camY);
    player->draw(p, TILE_SIZE);
    p.restore();

    // Fog of war overlay (limited visibility + unexplored darkening).
    const int px = player->getX();
    const int py = player->getY();
    if (!m_explored.isEmpty() && py >= 0 && py < m_explored.size() && px >= 0 && px < m_explored.at(py).size()) {
        const int r = VISIBILITY_RADIUS_TILES;
        for (int y = qMax(0, py - r); y <= qMin(level->getHeight() - 1, py + r); ++y) {
            for (int x = qMax(0, px - r); x <= qMin(level->getWidth() - 1, px + r); ++x) {
                const int dx = x - px;
                const int dy = y - py;
                if (dx * dx + dy * dy <= r * r) {
                    m_explored[y][x] = true;
                }
            }
        }
    }

    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            const bool explored = !m_explored.isEmpty() ? m_explored.at(y).at(x) : false;
            const int dx = x - player->getX();
            const int dy = y - player->getY();
            const bool visibleNow = (dx * dx + dy * dy) <= (VISIBILITY_RADIUS_TILES * VISIBILITY_RADIUS_TILES);

            QColor overlay;
            if (!explored) {
                overlay = QColor(0, 0, 0, 235);
            } else if (!visibleNow) {
                overlay = QColor(0, 0, 0, 170);
            } else {
                continue;
            }

            const int screenX = x * TILE_SIZE - camX;
            const int screenY = y * TILE_SIZE - camY;
            p.fillRect(QRect(screenX, screenY, TILE_SIZE, TILE_SIZE), overlay);
        }
    }
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
    // Phase 2+: viewport stays fixed; camera determines what part of the map is visible.
    setFixedSize(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
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
