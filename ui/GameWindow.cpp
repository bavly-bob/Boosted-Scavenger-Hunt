#include "GameWindow.h"

#include "Game.h"
#include "GameOverOverlay.h"
#include "GameObject.h"
#include "InputHandler.h"
#include "Level.h"
#include "LevelLoader.h"
#include "PauseOverlay.h"
#include "Player.h"
#include "Wall.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QTimer>


namespace {
QString formatTime(int seconds)
{
    const int m = seconds / 60;
    const int s = seconds % 60;
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
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
    for (const QString& c : candidates) {
        if (QDir(c).exists()) return QDir(c).absolutePath();
    }
    return QDir::current().absolutePath();
}

QString findAssetsDir()
{
    const QStringList candidates = {
        QDir::current().filePath("assets"),
        QDir::current().filePath("../assets"),
        QDir(QCoreApplication::applicationDirPath()).filePath("assets"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../assets"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../../assets"),
    };
    for (const QString& c : candidates) {
        if (QDir(c).exists()) return QDir(c).absolutePath();
    }
    return QString();
}

// Returns a colour to tint each tile based on its CellType.
QColor tileColour(CellType ct, int x, int y)
{
    switch (ct) {
    case CellType::Chamber:
        // Warm stone-floor checkerboard
        return ((x + y) % 2 == 0) ? QColor(58, 50, 42, 220) : QColor(52, 44, 36, 220);
    case CellType::Corridor:
        // Darker, narrow-path look
        return ((x + y) % 2 == 0) ? QColor(38, 34, 30, 240) : QColor(32, 28, 24, 240);
    case CellType::Wall:
    case CellType::HiddenWall:
        return QColor(0, 0, 0, 0); // walls are drawn via Wall::draw(); skip fill here
    case CellType::TreasureRoom:
        return QColor(160, 130, 40, 180);
    case CellType::Coin:
        return QColor(200, 180, 50, 100);
    case CellType::ClueTrigger:
        return QColor(60, 120, 200, 100);
    default:
        return ((x + y) % 2 == 0) ? QColor(255, 255, 255, 12) : QColor(0, 0, 0, 12);
    }
}

// Draw tile border / grout line
void drawTileBorder(QPainter& p, const QRect& cell, CellType ct)
{
    switch (ct) {
    case CellType::Chamber:
        p.setPen(QPen(QColor(35, 28, 22, 80), 1));
        p.drawRect(cell);
        break;
    case CellType::Corridor:
        p.setPen(QPen(QColor(20, 16, 12, 100), 1));
        p.drawRect(cell);
        break;
    default:
        break;
    }
}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
GameWindow::GameWindow(QWidget *parent)
    : QWidget(parent),
      m_game(new Game(this)),
      m_pauseOverlay(new PauseOverlay(this)),
      m_gameOverOverlay(new GameOverOverlay(this)),
      m_renderTimer(new QTimer(this)),
      m_cameraPx(0.0, 0.0),
      m_levelsConfigured(false),
      m_assetsLoaded(false)
{
    setWindowTitle("Scavenger Hunt");
    setFocusPolicy(Qt::StrongFocus);

    m_pauseOverlay->hide();
    m_gameOverOverlay->hide();

    connect(m_game, &Game::gameUpdated,       this, QOverload<>::of(&GameWindow::update));
    connect(m_game, &Game::clueRevealed,      this, &GameWindow::onClueRevealed);
    connect(m_game, &Game::wallOpened,        this, &GameWindow::onWallOpened);
    connect(m_game, &Game::treasureUnlocked,  this, &GameWindow::onTreasureUnlocked);
    connect(m_game, &Game::gameOver,          this, &GameWindow::onGameOver);

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
        resetExplored();
        resizeToCurrentLevel();
        injectSprites();
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
        resetExplored();
        resizeToCurrentLevel();
        injectSprites();
        setFocus();
    });
    connect(m_gameOverOverlay, &GameOverOverlay::restartRequested, this, [this]() {
        m_gameOverOverlay->hide();
        m_game->restartLevel();
        resetExplored();
        resizeToCurrentLevel();
        injectSprites();
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

// ─────────────────────────────────────────────────────────────────────────────
// Asset loading
// ─────────────────────────────────────────────────────────────────────────────
void GameWindow::loadAssets()
{
    if (m_assetsLoaded) return;
    m_assetsLoaded = true;

    const QString assetsDir = findAssetsDir();
    if (assetsDir.isEmpty()) return;

    // Load wall sprite sheet (128×32 → 4 variants of 32×32).
    // Slice horizontally and insert each variant individually.
    const QString wallSheet = assetsDir + "/wall_sprites.png";
    QPixmap wallPix(wallSheet);
    if (!wallPix.isNull()) {
        const int tileW = wallPix.width() / 4;
        const int tileH = wallPix.height();
        for (int v = 0; v < 4; ++v) {
            QPixmap tile = wallPix.copy(v * tileW, 0, tileW, tileH)
                               .scaled(TILE_SIZE, TILE_SIZE,
                                       Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
            m_spriteManager.loadPixmap(QString("wall_%1").arg(v), tile);
        }
    }

    // Load player sprite sheet (128×160 → 5 rows × 4 frames of 32×32).
    const QString playerSheet = assetsDir + "/player_sprites.png";
    if (QFile::exists(playerSheet)) {
        m_spriteManager.load("player_sheet", playerSheet);

        // Register animation clips — each row in the sheet is one state.
        const QStringList clipNames = {
            "player_idle", "player_move_down", "player_move_up",
            "player_move_left", "player_move_right"
        };
        for (int row = 0; row < clipNames.size(); ++row) {
            AnimationClip c;
            c.spriteKey   = "player_sheet";
            c.frameCount  = 4;
            c.frameWidth  = 32;
            c.frameHeight = 32;
            c.srcY        = row * 32;
            c.fps         = 8;
            m_spriteManager.registerClip(clipNames[row], c);
        }
    }

    const QString enemySheet = assetsDir + "/enemy_sprites.png";
    if (QFile::exists(enemySheet)) {
        m_spriteManager.load("enemy_sheet", enemySheet);

        const QStringList enemyClipNames = {
            "enemy_idle", "enemy_move_down", "enemy_move_up",
            "enemy_move_left", "enemy_move_right", "enemy_die"
        };
        for (int row = 0; row < enemyClipNames.size(); ++row) {
            AnimationClip c;
            c.spriteKey   = "enemy_sheet";
            c.frameCount  = 4;
            c.frameWidth  = 32;
            c.frameHeight = 32;
            c.srcY        = row * 32;
            c.fps         = 8;
            m_spriteManager.registerClip(enemyClipNames[row], c);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sprite injection
// ─────────────────────────────────────────────────────────────────────────────
void GameWindow::injectSprites()
{
    const Level*  level  = m_game->level();
    const Player* player = m_game->player();

    if (player) {
        // const_cast: we own the player through Game and need to configure it.
        const_cast<Player*>(player)->setSpriteManager(&m_spriteManager);
    }

    if (level) {
        for (GameObject* obj : level->getObjects()) {
            if (Wall* w = dynamic_cast<Wall*>(obj)) {
                w->setSpriteManager(&m_spriteManager);
            }
        }
        for (Enemy* enemy : level->getEnemies()) {
            enemy->setSpriteManager(&m_spriteManager);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void GameWindow::startNewGame(Difficulty difficulty)
{
    loadAssets();
    ensureLevelsConfigured();
    m_statusText.clear();
    m_pauseOverlay->hide();
    m_gameOverOverlay->hide();

    m_game->startNewGame(difficulty);
    resetExplored();
    resizeToCurrentLevel();
    snapCameraToPlayer();
    injectSprites();

    show();
    setFocus();
}

void GameWindow::snapCameraToPlayer()
{
    const Level*  level  = m_game->level();
    const Player* player = m_game->player();
    if (!level || !player) { m_cameraPx = QPointF(0.0, 0.0); return; }

    const qreal playerPxX = player->getX() * TILE_SIZE + TILE_SIZE / 2.0;
    const qreal playerPxY = player->getY() * TILE_SIZE + TILE_SIZE / 2.0;

    const int mapPixelW = level->getWidth()  * TILE_SIZE;
    const int mapPixelH = level->getHeight() * TILE_SIZE;
    const int viewPixelW = width();
    const int viewPixelH = height() - HUD_HEIGHT;

    qreal camX = qBound<qreal>(0.0, playerPxX - viewPixelW / 2.0,
                               qMax<qreal>(0.0, mapPixelW - viewPixelW));
    qreal camY = qBound<qreal>(0.0, playerPxY - viewPixelH / 2.0,
                               qMax<qreal>(0.0, mapPixelH - viewPixelH));
    m_cameraPx = QPointF(camX, camY);
}

// ─────────────────────────────────────────────────────────────────────────────
// Painting
// ─────────────────────────────────────────────────────────────────────────────
void GameWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // ── Background gradient ────────────────────────────────────────────────
    QLinearGradient grad(0, 0, width(), height());
    grad.setColorAt(0.0, QColor(18,  8, 48));
    grad.setColorAt(0.4, QColor(12, 40, 55));
    grad.setColorAt(1.0, QColor( 8, 16, 28));
    p.fillRect(rect(), grad);

    // ── HUD ────────────────────────────────────────────────────────────────
    {
        QLinearGradient hudGrad(0, 0, 0, HUD_HEIGHT);
        hudGrad.setColorAt(0.0, QColor(0, 0, 0, 180));
        hudGrad.setColorAt(1.0, QColor(0, 0, 0, 60));
        p.fillRect(QRect(0, 0, width(), HUD_HEIGHT), hudGrad);

        // Separator line
        p.setPen(QPen(QColor(100, 80, 200, 120), 1));
        p.drawLine(0, HUD_HEIGHT - 1, width(), HUD_HEIGHT - 1);

        const QString levelName = m_game->currentLevelName();
        const QString scoreText = QString("Score: %1").arg(m_game->score());
        const int coins = m_game->coinsCollected();
        const QString timeText = formatTime(m_game->timeRemaining());

        // Level name
        QFont titleFont = p.font();
        titleFont.setBold(true);
        titleFont.setPointSize(11);
        p.setFont(titleFont);
        p.setPen(QColor(220, 200, 255));
        p.drawText(QRect(12, 6, width() - 24, 24),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   levelName.isEmpty() ? "Scavenger Hunt" : levelName);

        QFont infoFont = p.font();
        infoFont.setBold(false);
        infoFont.setPointSize(9);
        p.setFont(infoFont);

        // Score
        p.setPen(QColor(180, 220, 180));
        p.drawText(QRect(12, 32, 160, 18), Qt::AlignLeft | Qt::AlignVCenter, scoreText);

        // Coins — draw coloured coin icons
        const int coinX = width() / 2 - 60;
        p.setPen(QColor(200, 200, 200));
        p.drawText(QRect(coinX, 32, 120, 18), Qt::AlignLeft | Qt::AlignVCenter, "Coins: ");
        for (int i = 0; i < 4; ++i) {
            const QColor coinColor = (i < coins) ? QColor(255, 210, 40) : QColor(80, 80, 80);
            p.setBrush(coinColor);
            p.setPen(QPen(coinColor.darker(150), 1));
            p.drawEllipse(coinX + 58 + i * 20, 36, 12, 12);
        }

        // Timer
        const int timeRemaining = m_game->timeRemaining();
        const QColor timeColor = (timeRemaining > 30) ? QColor(200, 220, 200)
                                 : (timeRemaining > 10) ? QColor(240, 200, 80)
                                 : QColor(240, 80, 80);
        p.setPen(timeColor);
        p.drawText(QRect(width() - 120, 32, 108, 18),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString("⏱ %1").arg(timeText));

        // Status text
        if (!m_statusText.isEmpty()) {
            QFont statusFont = p.font();
            statusFont.setItalic(true);
            p.setFont(statusFont);
            p.setPen(QColor(230, 200, 120));
            p.drawText(QRect(12, 52, width() - 24, 16),
                       Qt::AlignLeft | Qt::AlignVCenter, m_statusText);
        }

        // Draw HUD buttons (Restart and Quit)
        p.setPen(QPen(QColor(200, 200, 200), 1));
        
        // Restart Button
        QRect restartBtn(width() - 210, 6, 80, 24);
        p.setBrush(QColor(60, 60, 80));
        p.drawRoundedRect(restartBtn, 4, 4);
        p.drawText(restartBtn, Qt::AlignCenter, "Restart");

        // Quit Button
        QRect quitBtn(width() - 120, 6, 80, 24);
        p.setBrush(QColor(80, 40, 40));
        p.drawRoundedRect(quitBtn, 4, 4);
        p.drawText(quitBtn, Qt::AlignCenter, "Quit");
    }

    const Level*  level  = m_game->level();
    const Player* player = m_game->player();
    if (!level || !player) return;

    // ── Camera ────────────────────────────────────────────────────────────
    p.save();
    p.translate(0, HUD_HEIGHT);

    const int mapPixelW = level->getWidth()  * TILE_SIZE;
    const int mapPixelH = level->getHeight() * TILE_SIZE;
    const int viewPixelW = width();
    const int viewPixelH = height() - HUD_HEIGHT;

    const qreal playerPxX = player->getX() * TILE_SIZE + TILE_SIZE / 2.0;
    const qreal playerPxY = player->getY() * TILE_SIZE + TILE_SIZE / 2.0;

    qreal targetCamX = qBound<qreal>(0.0, playerPxX - viewPixelW / 2.0,
                                     qMax<qreal>(0.0, mapPixelW - viewPixelW));
    qreal targetCamY = qBound<qreal>(0.0, playerPxY - viewPixelH / 2.0,
                                     qMax<qreal>(0.0, mapPixelH - viewPixelH));
    m_cameraPx.setX(m_cameraPx.x() + (targetCamX - m_cameraPx.x()) * CAMERA_LERP);
    m_cameraPx.setY(m_cameraPx.y() + (targetCamY - m_cameraPx.y()) * CAMERA_LERP);

    const int camX = static_cast<int>(m_cameraPx.x());
    const int camY = static_cast<int>(m_cameraPx.y());

    const int startX = qMax(0, camX / TILE_SIZE);
    const int startY = qMax(0, camY / TILE_SIZE);
    const int endX   = qMin(level->getWidth()  - 1, (camX + viewPixelW + TILE_SIZE - 1) / TILE_SIZE);
    const int endY   = qMin(level->getHeight() - 1, (camY + viewPixelH + TILE_SIZE - 1) / TILE_SIZE);

    // ── Base tile pass ────────────────────────────────────────────────────
    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            const int screenX = x * TILE_SIZE - camX;
            const int screenY = y * TILE_SIZE - camY;
            const QRect cell(screenX, screenY, TILE_SIZE, TILE_SIZE);
            const CellType ct = static_cast<CellType>(level->tileAt(x, y));

            if (ct == CellType::Wall || ct == CellType::HiddenWall) {
                // Draw wall tiles directly (no Wall object required in chamber format).
                static const QColor wallColors[] = {
                    QColor(62, 62, 72),  // default stone
                    QColor(48, 68, 48),  // mossy
                    QColor(80, 60, 44),  // brown brick
                    QColor(44, 44, 66),  // slate
                };
                // Use a deterministic variant per tile position for variety.
                const QColor base = wallColors[((x * 3 + y * 7) & 0xFF) % 4];
                p.fillRect(cell, base);
                p.setPen(QPen(base.lighter(130), 1));
                p.drawLine(cell.topLeft(),   cell.topRight());
                p.drawLine(cell.topLeft(),   cell.bottomLeft());
                p.setPen(QPen(base.darker(155), 1));
                p.drawLine(cell.bottomLeft(),  cell.bottomRight());
                p.drawLine(cell.topRight(),    cell.bottomRight());
                // Horizontal mortar line
                p.setPen(QPen(base.darker(125), 1));
                const int halfH = cell.top() + TILE_SIZE / 2;
                p.drawLine(cell.left() + 1, halfH, cell.right() - 1, halfH);
                continue;
            }

            const QColor base = tileColour(ct, x, y);
            p.fillRect(cell, base);
            drawTileBorder(p, cell, ct);
        }
    }


    // ── Object pass (walls, coins, triggers, player) ──────────────────────
    p.save();
    p.translate(-camX, -camY);
    for (GameObject* obj : level->getObjects()) {
        if (!obj) continue;
        const int ox = obj->getX(), oy = obj->getY();
        if (ox >= startX && ox <= endX && oy >= startY && oy <= endY) {
            obj->draw(p, TILE_SIZE);
        }
    }
    player->draw(p, TILE_SIZE);
    p.restore();

    // ── Fog of war ────────────────────────────────────────────────────────
    const int px = player->getX(), py = player->getY();
    const int r  = VISIBILITY_RADIUS_TILES;

    // Mark newly visible tiles as explored
    if (!m_explored.isEmpty()) {
        for (int vy = qMax(0, py - r); vy <= qMin(level->getHeight() - 1, py + r); ++vy) {
            for (int vx = qMax(0, px - r); vx <= qMin(level->getWidth() - 1, px + r); ++vx) {
                const int dx = vx - px, dy = vy - py;
                if (dx * dx + dy * dy <= r * r)
                    m_explored[vy][vx] = true;
            }
        }
    }

    for (int fy = startY; fy <= endY; ++fy) {
        for (int fx = startX; fx <= endX; ++fx) {
            const bool explored   = !m_explored.isEmpty() && m_explored.at(fy).at(fx);
            const int  dx = fx - px, dy = fy - py;
            const bool visibleNow = (dx * dx + dy * dy) <= (r * r);

            QColor overlay;
            if (!explored) {
                overlay = QColor(0, 0, 0, 240);
            } else if (!visibleNow) {
                overlay = QColor(0, 0, 0, 160);
            } else {
                // Soft radial vignette within visible area
                const double dist = std::sqrt(dx * dx + dy * dy);
                const int alpha   = static_cast<int>((dist / r) * 80.0);
                overlay = QColor(0, 0, 0, qBound(0, alpha, 80));
            }

            const int screenX = fx * TILE_SIZE - camX;
            const int screenY = fy * TILE_SIZE - camY;
            p.fillRect(QRect(screenX, screenY, TILE_SIZE, TILE_SIZE), overlay);
        }
    }

    // ── Player glow highlight (visible area indicator) ────────────────────
    {
        const int gScreenX = px * TILE_SIZE - camX + TILE_SIZE / 2;
        const int gScreenY = py * TILE_SIZE - camY + TILE_SIZE / 2;
        QRadialGradient glow(gScreenX, gScreenY, r * TILE_SIZE);
        glow.setColorAt(0.0, QColor(100, 150, 255, 18));
        glow.setColorAt(0.6, QColor(60, 100, 200, 8));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(QRect(gScreenX - r * TILE_SIZE, gScreenY - r * TILE_SIZE,
                         r * TILE_SIZE * 2, r * TILE_SIZE * 2), glow);
    }

    p.restore(); // end of HUD translate
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────
void GameWindow::keyPressEvent(QKeyEvent *event)
{
    if (!event) return;

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

void GameWindow::mousePressEvent(QMouseEvent *event)
{
    if (!event) return;

    if (m_game->state() == GameState::PLAYING) {
        // Check button rects
        QRect restartBtn(width() - 210, 6, 80, 24);
        QRect quitBtn(width() - 120, 6, 80, 24);

        if (restartBtn.contains(event->pos())) {
            m_game->restartLevel();
            resetExplored();
            resizeToCurrentLevel();
            injectSprites();
            setFocus();
            return;
        }

        if (quitBtn.contains(event->pos())) {
            m_game->pause();
            m_gameOverOverlay->hide();
            hide();
            emit quitToMainMenuRequested();
            return;
        }
    }

    QWidget::mousePressEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────
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
    m_statusText = "Treasure unlocked! Find the treasure room!";
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

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────
void GameWindow::ensureLevelsConfigured()
{
    if (m_levelsConfigured) return;
    const QString levelsDir = findLevelsDir();
    m_game->setLevelFiles(LevelLoader::getAvailableLevels(levelsDir));
    m_levelsConfigured = true;
}

void GameWindow::resizeToCurrentLevel()
{
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

void GameWindow::resetExplored()
{
    if (const Level* level = m_game->level()) {
        m_explored = QVector<QVector<bool>>(level->getHeight(),
                                            QVector<bool>(level->getWidth(), false));
    } else {
        m_explored.clear();
    }
}
