#include "GameWindow.h"

#include "Game.h"
#include "GameOverOverlay.h"
#include "GameObject.h"
#include "Coin.h"
#include "Enemy.h"
#include "InputHandler.h"
#include "Level.h"
#include "LevelLoader.h"
#include "PauseOverlay.h"
#include "Player.h"
#include "Wall.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QTimer>
#include <QPushButton>


namespace {
QString formatTime(int seconds)
{
    const int minutes = seconds / 60;
    const int remainingSeconds = seconds % 60;
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(remainingSeconds, 2, 10, QChar('0'));
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

QString firstExistingFile(const QStringList& paths)
{
    for (const QString& path : paths) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return QString();
}

bool loadSheetWithFallback(QPixmap& out, const QStringList& candidatePaths)
{
    const QString path = firstExistingFile(candidatePaths);
    if (path.isEmpty()) {
        return false;
    }
    QPixmap pix(path);
    if (pix.isNull()) {
        return false;
    }
    out = pix;
    return true;
}

bool loadTileWithFallback(QPixmap& out, const QStringList& candidatePaths, int tileSize)
{
    const QString path = firstExistingFile(candidatePaths);
    if (path.isEmpty()) {
        return false;
    }

    QPixmap pix(path);
    if (pix.isNull()) {
        return false;
    }

    if (pix.width() != tileSize || pix.height() != tileSize) {
        pix = pix.scaled(tileSize, tileSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    out = pix;
    return true;
}

QPixmap tintedPixmap(const QPixmap& src, const QColor& tint, int alpha)
{
    if (src.isNull()) {
        return QPixmap();
    }

    QPixmap out = src;
    QPainter painter(&out);
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    QColor overlay = tint;
    overlay.setAlpha(alpha);
    painter.fillRect(out.rect(), overlay);
    painter.end();
    return out;
}

quint32 tileHash(int x, int y, quint32 salt = 0u)
{
    quint32 h = static_cast<quint32>(x) * 0x9e3779b9u;
    h ^= static_cast<quint32>(y) * 0x85ebca6bu;
    h ^= salt * 0xc2b2ae35u;
    h ^= (h >> 16);
    h *= 0x7feb352du;
    h ^= (h >> 15);
    return h;
}

QColor tileColour(CellType ct, int x, int y)
{
    switch (ct) {
    case CellType::Chamber:
        return ((x + y) % 2 == 0) ? QColor(58, 50, 42, 220) : QColor(52, 44, 36, 220);
    case CellType::Corridor:
        return ((x + y) % 2 == 0) ? QColor(38, 34, 30, 240) : QColor(32, 28, 24, 240);
    case CellType::Wall:
    case CellType::HiddenWall:
        return QColor(0, 0, 0, 0);
    case CellType::TreasureRoom:
        return QColor(160, 130, 40, 180);
    case CellType::OpenedSecretFloor:
        return QColor(76, 150, 86, 220);
    case CellType::Coin:
        return QColor(200, 180, 50, 100);
    case CellType::ClueTrigger:
        return QColor(60, 120, 200, 100);
    default:
        return ((x + y) % 2 == 0) ? QColor(255, 255, 255, 12) : QColor(0, 0, 0, 12);
    }
}

void drawTileBorder(QPainter& p, const QRect& cell, CellType ct)
{
    switch (ct) {
    case CellType::Chamber:
        p.setPen(QPen(QColor(30, 22, 16, 70), 1));
        p.drawLine(cell.topLeft(), cell.topRight());
        p.drawLine(cell.topLeft(), cell.bottomLeft());
        break;
    case CellType::Corridor:
        p.setPen(QPen(QColor(18, 14, 10, 90), 1));
        p.drawLine(cell.topLeft(), cell.topRight());
        p.drawLine(cell.topLeft(), cell.bottomLeft());
        break;
    case CellType::OpenedSecretFloor:
        p.setPen(QPen(QColor(36, 90, 48, 120), 1));
        p.drawLine(cell.topLeft(), cell.topRight());
        p.drawLine(cell.bottomLeft(), cell.bottomRight());
        break;
    default:
        break;
    }
}
} // namespace

GameWindow::GameWindow(QWidget *parent)
    : QWidget(parent),
      m_game(new Game(this)),
      m_pauseOverlay(new PauseOverlay(this)),
      m_gameOverOverlay(new GameOverOverlay(this)),
      m_renderTimer(new QTimer(this)),
      m_cameraPx(0.0, 0.0),
      m_levelsConfigured(false),
      m_assetsLoaded(false),
      m_statusIsAiHint(false),
      m_hudRestartBtn(new QPushButton("Restart", this)),
      m_hudQuitBtn(new QPushButton("Quit", this)),
      m_statusDurationMs(3000),
      m_prevCoins(0),
      m_objectiveFlashActive(false)
{
    setWindowTitle("Scavenger Hunt");
    setFocusPolicy(Qt::StrongFocus);
    m_frameTimer.start();
    m_deltaMs = 16.0f;

    m_pauseOverlay->hide();
    m_gameOverOverlay->hide();

    m_hudRestartBtn->setCursor(Qt::PointingHandCursor);
    m_hudRestartBtn->setStyleSheet(
        "QPushButton { background: rgba(60,60,80,0.8); color: white; border-radius: 4px; font-size: 13px; }"
        "QPushButton:hover { background: rgba(80,80,100,0.9); border: 1px solid white; }"
        "QPushButton:pressed { background: rgba(40,40,60,0.9); }"
    );
    m_hudQuitBtn->setCursor(Qt::PointingHandCursor);
    m_hudQuitBtn->setStyleSheet(
        "QPushButton { background: rgba(80,40,40,0.8); color: white; border-radius: 4px; font-size: 13px; }"
        "QPushButton:hover { background: rgba(100,50,50,0.9); border: 1px solid white; }"
        "QPushButton:pressed { background: rgba(60,30,30,0.9); }"
    );
    connect(m_hudRestartBtn, &QPushButton::clicked, this, [this](){
        m_game->restartLevel();
        setFocus();
    });
    connect(m_hudQuitBtn, &QPushButton::clicked, this, [this](){
        m_game->pause();
        m_game->saveGame("save.json");
        m_gameOverOverlay->hide();
        hide();
        emit quitToMainMenuRequested();
    });

    connect(m_game, &Game::gameUpdated,       this, QOverload<>::of(&GameWindow::update));
    connect(m_game, &Game::levelChanged,      this, &GameWindow::onLevelChanged);
    connect(m_game, &Game::clueRevealed,      this, &GameWindow::onClueRevealed);
    connect(m_game, &Game::wallOpened,        this, &GameWindow::onWallOpened);
    connect(m_game, &Game::treasureUnlocked,  this, &GameWindow::onTreasureUnlocked);
    connect(m_game, &Game::gameOver,          this, &GameWindow::onGameOver);

    m_renderTimer->setInterval(16);
    connect(m_renderTimer, &QTimer::timeout, this, [this]() {
        m_deltaMs = static_cast<float>(m_frameTimer.restart());
        if (m_game->state() == GameState::PLAYING) {
            const Player* p = m_game->player();
            if (p) {
                const_cast<Player*>(p)->updateMovement(m_deltaMs / 1000.0f);
                const_cast<Player*>(p)->advanceAnimation();
            }
        }
        update();
    });
    m_renderTimer->start();

    connect(m_pauseOverlay, &PauseOverlay::resumeRequested, this, [this]() {
        hidePauseOverlay();
        m_game->resume();
        setFocus();
    });
    connect(m_pauseOverlay, &PauseOverlay::restartRequested, this, [this]() {
        hidePauseOverlay();
        m_statusText.clear();
        m_statusIsAiHint = false;
        m_game->restartLevel();
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
        m_statusText.clear();
        m_statusIsAiHint = false;
        m_game->nextLevel();
        setFocus();
    });
    connect(m_gameOverOverlay, &GameOverOverlay::restartRequested, this, [this]() {
        m_gameOverOverlay->hide();
        m_statusText.clear();
        m_statusIsAiHint = false;
        m_game->restartLevel();
        setFocus();
    });
    connect(m_gameOverOverlay, &GameOverOverlay::mainMenuRequested, this, [this]() {
        m_gameOverOverlay->hide();
        m_game->pause();
        hide();
        emit quitToMainMenuRequested();
    });

    setMinimumSize(800, 600);
    resize(1280, 960);
    layoutHudButtons();
}
static QRect alphaBounds(const QImage& img)
{
    int minX = img.width();
    int minY = img.height();
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(img.pixel(x, y)) > 8) {
                minX = qMin(minX, x);
                minY = qMin(minY, y);
                maxX = qMax(maxX, x);
                maxY = qMax(maxY, y);
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        return QRect();
    }
    return QRect(QPoint(minX, minY), QPoint(maxX, maxY));
}

static QPixmap makePlayerFrame(const QImage& source, int tileSize)
{
    if (source.isNull()) {
        return QPixmap();
    }

    QImage rgba = source.convertToFormat(QImage::Format_ARGB32);
    const QRect bounds = alphaBounds(rgba);
    if (bounds.isValid()) {
        rgba = rgba.copy(bounds);
    }

    const QSize target = rgba.size().scaled(tileSize - 2, tileSize - 2, Qt::KeepAspectRatio);
    QPixmap frame(tileSize, tileSize);
    frame.fill(Qt::transparent);

    QPainter painter(&frame);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QRect dst((tileSize - target.width()) / 2,
                    (tileSize - target.height()) / 2,
                    target.width(),
                    target.height());
    painter.drawImage(dst, rgba);
    painter.end();

    return frame;
}

static bool loadPlayerDirectionFrames(SpriteManager& sm,
                                      const QString&  assetsDir,
                                      const char*     dirName,
                                      const char*     clipKey,
                                      int             tileSize)
{
    const QString folder = assetsDir + "/player/" + dirName;
    QVector<QPixmap> frames;
    for (int i = 0; i < 8; ++i) {
        const QString path = folder + QString("/frame_%1.png").arg(i);
        if (!QFile::exists(path)) break;
        QImage img(path);
        if (img.isNull()) break;
        if (!img.hasAlphaChannel()) {
            QColor bgColor = img.pixelColor(0, 0);
            QPixmap tmp = QPixmap::fromImage(img);
            tmp.setMask(tmp.createMaskFromColor(bgColor, Qt::MaskOutColor));
            img = tmp.toImage();
        }

        QPixmap frame = makePlayerFrame(img, tileSize);
        if (!frame.isNull()) {
            frames.append(frame);
        }
    }

    if (frames.isEmpty()) return false;
    for (int i = 0; i < frames.size(); ++i) {
        sm.loadPixmap(QString("%1_frame_%2").arg(clipKey).arg(i), frames[i]);
    }
    AnimationClip c;
    c.spriteKey   = QString("%1_frame_0").arg(clipKey);
    c.frameCount  = frames.size();
    c.frameWidth  = tileSize;
    c.frameHeight = tileSize;
    c.srcY        = 0;
    c.fps         = 8;
    sm.registerClip(QString(clipKey), c);
    return true;
}

void GameWindow::loadAssets()
{
    if (m_assetsLoaded) return;
    m_assetsLoaded = true;

    const QString assetsDir = findAssetsDir();
    if (assetsDir.isEmpty()) return;
    m_floorTiles.clear();
    m_wallTiles.clear();
    m_openedSecretFloorTile = QPixmap();
    m_treasurePedestalTile = QPixmap();
    m_pressurePlateTile = QPixmap();
    m_vignetteOverlay = QPixmap();
    m_fogPatchOverlay = QPixmap();
    m_floorTileSheet = QPixmap();
    m_wallTileSheet = QPixmap();
    m_propsTileSheet = QPixmap();

    const QString dungeonDir = assetsDir + "/dungeon";

    const QStringList floorFiles = {
        dungeonDir + "/tiles/floor_stone_base_16.png",
        dungeonDir + "/tiles/floor_stone_cracked_16.png",
        dungeonDir + "/tiles/floor_moss_patch_16.png",
        dungeonDir + "/tiles/floor_rubble_16.png",
        dungeonDir + "/tiles/floor_water_edge_16.png",
    };
    for (const QString& file : floorFiles) {
        QPixmap pix;
        if (loadTileWithFallback(pix, {file}, TILE_SIZE)) {
            m_floorTiles.append(pix);
        }
    }
    if (!m_floorTiles.isEmpty()) {
        m_openedSecretFloorTile = tintedPixmap(m_floorTiles.at(0), QColor(66, 200, 88), 110);
    }

    const QStringList wallFiles = {
        dungeonDir + "/walls/wall_stone_block_16.png",
        dungeonDir + "/walls/wall_stone_cracked_16.png",
        dungeonDir + "/walls/wall_corner_inner_16.png",
        dungeonDir + "/walls/wall_corner_outer_16.png",
        dungeonDir + "/walls/wall_pillar_16.png",
        dungeonDir + "/walls/door_arch_closed_16.png",
        dungeonDir + "/walls/door_arch_open_16.png",
    };
    for (int i = 0; i < wallFiles.size(); ++i) {
        QPixmap pix;
        if (loadTileWithFallback(pix, {wallFiles.at(i)}, TILE_SIZE)) {
            m_wallTiles.append(pix);
            m_spriteManager.loadPixmap(QString("wall_%1").arg(i), pix);
        }
    }

    loadTileWithFallback(m_treasurePedestalTile,
                         {dungeonDir + "/props/treasure_pedestal_16.png"},
                         TILE_SIZE);
    if (loadTileWithFallback(m_pressurePlateTile,
                             {dungeonDir + "/props/pressure_plate_16.png"},
                             TILE_SIZE)) {
        m_spriteManager.loadPixmap("pressure_plate", m_pressurePlateTile);
        m_spriteManager.loadPixmap("pressure_plate_on",
                                   tintedPixmap(m_pressurePlateTile, QColor(120, 220, 130), 70));
    }

    loadSheetWithFallback(m_vignetteOverlay, {
        dungeonDir + "/lighting/vignette_soft_256.png"
    });
    loadSheetWithFallback(m_fogPatchOverlay, {
        dungeonDir + "/lighting/fog_patch_soft_64.png"
    });

    loadSheetWithFallback(m_floorTileSheet, {
        dungeonDir + "/tiles/dungeon_floor_tiles.png"
    });
    loadSheetWithFallback(m_wallTileSheet, {
        dungeonDir + "/walls/dungeon_wall_tiles.png"
    });
    loadSheetWithFallback(m_propsTileSheet, {
        dungeonDir + "/props/dungeon_props.png"
    });

    if (m_wallTiles.isEmpty() && !m_wallTileSheet.isNull()) {
        const int srcTileSize = 32;
        const int cols = m_wallTileSheet.width() / srcTileSize;
        for (int v = 0; v < 4; ++v) {
            if (v >= cols) {
                break;
            }
            QPixmap tile = m_wallTileSheet.copy(v * srcTileSize, 0, srcTileSize, srcTileSize)
                               .scaled(TILE_SIZE, TILE_SIZE,
                                       Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
            m_wallTiles.append(tile);
            m_spriteManager.loadPixmap(QString("wall_%1").arg(v), tile);
        }
    }
    const struct { const char* folder; const char* clip; } playerDirs[] = {
        { "down",  "player_move_down"  },
        { "up",    "player_move_up"    },
        { "left",  "player_move_left"  },
        { "right", "player_move_right" },
        { "idle",  "player_idle"       },
    };

    bool anyPlayerFrameLoaded = false;
    for (const auto& entry : playerDirs) {
        if (loadPlayerDirectionFrames(m_spriteManager, assetsDir,
                                      entry.folder, entry.clip, TILE_SIZE)) {
            anyPlayerFrameLoaded = true;
        }
    }
    if (!anyPlayerFrameLoaded) {
        const QString playerSheet = assetsDir + "/player_sprites.png";
        if (QFile::exists(playerSheet)) {
            QImage playerImg(playerSheet);
            if (!playerImg.isNull()) {
                QPixmap fullPix;
                if (playerImg.hasAlphaChannel()) {
                    fullPix = QPixmap::fromImage(playerImg);
                } else {
                    fullPix = QPixmap::fromImage(playerImg);
                    QColor bgColor = playerImg.pixelColor(0, 0);
                    fullPix.setMask(fullPix.createMaskFromColor(bgColor, Qt::MaskOutColor));
                }
                m_spriteManager.loadPixmap("player_sheet", fullPix);
            } else {
                m_spriteManager.load("player_sheet", playerSheet);
            }

            const QPixmap& px = m_spriteManager.sprite("player_sheet");
            if (!px.isNull()) {
                const int ROWS = 5, COLS = 4;
                const int fw = px.width()  / COLS;
                const int fh = px.height() / ROWS;

                const struct { const char* name; int row; } sheetClips[] = {
                    { "player_move_down",  0 },
                    { "player_move_up",    1 },
                    { "player_move_left",  2 },
                    { "player_move_right", 3 },
                    { "player_idle",       4 },
                };
                for (const auto& e : sheetClips) {
                    AnimationClip c;
                    c.spriteKey   = "player_sheet";
                    c.frameCount  = COLS;
                    c.frameWidth  = fw;
                    c.frameHeight = fh;
                    c.srcY        = e.row * fh;
                    c.fps         = 8;
                    m_spriteManager.registerClip(QString(e.name), c);
                }
            }
        }
    }
    const QString enemySheet = assetsDir + "/enemy_sprites.png";
    if (QFile::exists(enemySheet)) {
        QImage enemyImg(enemySheet);
        if (!enemyImg.isNull()) {
            QPixmap fullPix;
            if (enemyImg.hasAlphaChannel()) {
                fullPix = QPixmap::fromImage(enemyImg);
            } else {
                QColor bgColor = enemyImg.pixelColor(0, 0);
                fullPix = QPixmap::fromImage(enemyImg);
                fullPix.setMask(fullPix.createMaskFromColor(bgColor, Qt::MaskOutColor));
            }
            m_spriteManager.loadPixmap("enemy_sheet", fullPix);
        } else {
            m_spriteManager.load("enemy_sheet", enemySheet);
        }

        const QPixmap& ex = m_spriteManager.sprite("enemy_sheet");
        if (!ex.isNull()) {
            const int ROWS = 6, COLS = 4;
            const int fw = ex.width()  / COLS;
            const int fh = ex.height() / ROWS;

            const struct { const char* name; int row; } enemyClips[] = {
                { "enemy_idle",        0 },
                { "enemy_move_down",   1 },
                { "enemy_move_up",     2 },
                { "enemy_move_left",   3 },
                { "enemy_move_right",  4 },
                { "enemy_die",         5 },
            };
            for (const auto& entry : enemyClips) {
                AnimationClip c;
                c.spriteKey   = "enemy_sheet";
                c.frameCount  = COLS;
                c.frameWidth  = fw;
                c.frameHeight = fh;
                c.srcY        = entry.row * fh;
                c.fps         = 8;
                m_spriteManager.registerClip(QString(entry.name), c);
            }
        }
    }
}

void GameWindow::injectSprites()
{
    const Level*  level  = m_game->level();
    const Player* player = m_game->player();

    if (player) {
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
void GameWindow::startNewGame(Difficulty difficulty)
{
    loadAssets();
    ensureLevelsConfigured();
    m_statusText.clear();
    m_statusIsAiHint = false;
    m_pauseOverlay->hide();
    m_gameOverOverlay->hide();

    m_game->startNewGame(difficulty);

    show();
    setFocus();
}

bool GameWindow::loadSavedGame(const QString& filepath)
{
    loadAssets();
    ensureLevelsConfigured();
    m_statusText.clear();
    m_statusIsAiHint = false;
    m_pauseOverlay->hide();
    m_gameOverOverlay->hide();

    if (!m_game->loadGame(filepath)) {
        return false;
    }

    show();
    setFocus();
    return true;
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
    const int viewPixelH = height() - hudHeight();

    qreal camX = qBound<qreal>(0.0, playerPxX - viewPixelW / 2.0,
                               qMax<qreal>(0.0, mapPixelW - viewPixelW));
    qreal camY = qBound<qreal>(0.0, playerPxY - viewPixelH / 2.0,
                               qMax<qreal>(0.0, mapPixelH - viewPixelH));
    m_cameraPx = QPointF(camX, camY);
}
void GameWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient grad(0, 0, width(), height());
    int levelIdx = m_game->currentLevelIndex();
    if (levelIdx % 3 == 0) {
        grad.setColorAt(0.0, QColor(18,  8, 48));
        grad.setColorAt(0.4, QColor(12, 40, 55));
        grad.setColorAt(1.0, QColor( 8, 16, 28));
    } else if (levelIdx % 3 == 1) {
        grad.setColorAt(0.0, QColor(8,  48, 18));
        grad.setColorAt(0.4, QColor(12, 55, 40));
        grad.setColorAt(1.0, QColor(8, 28, 16));
    } else {
        grad.setColorAt(0.0, QColor(48,  8, 18));
        grad.setColorAt(0.4, QColor(55, 12, 40));
        grad.setColorAt(1.0, QColor(28,  8, 16));
    }
    p.fillRect(rect(), grad);
    {
        p.save();
        p.setClipRect(QRect(0, 0, width(), hudHeight()));
        QLinearGradient hudGrad(0, 0, 0, hudHeight());
        hudGrad.setColorAt(0.0, QColor(0, 0, 0, 180));
        hudGrad.setColorAt(1.0, QColor(0, 0, 0, 60));
        p.fillRect(QRect(0, 0, width(), hudHeight()), hudGrad);
        p.setPen(QPen(QColor(100, 80, 200, 120), 1));
        p.drawLine(0, hudHeight() - 1, width(), hudHeight() - 1);

        const QString levelName = m_game->currentLevelName();
        const QString scoreText = QString("Score: %1").arg(m_game->score());
        const QString highScoreText = QString("High: %1").arg(m_game->highScore());
        const int coins = m_game->coinsCollected();
        const QString timeText = formatTime(m_game->timeRemaining());
        const QString runTimeText = formatTime(m_game->runTimeSeconds());
        
        QFont titleFont = p.font();
        titleFont.setBold(true);
        titleFont.setPointSize(13);
        p.setFont(titleFont);
        
        p.setPen(QColor(0, 0, 0, 150));
        p.drawText(QRect(13, 7, width() - 24, 24), Qt::AlignLeft | Qt::AlignVCenter, levelName.isEmpty() ? "Scavenger Hunt" : levelName);
        p.setPen(QColor(220, 200, 255));
        p.drawText(QRect(12, 6, width() - 24, 24), Qt::AlignLeft | Qt::AlignVCenter, levelName.isEmpty() ? "Scavenger Hunt" : levelName);

        QFont infoFont = p.font();
        infoFont.setBold(false);
        infoFont.setPointSize(11);
        p.setFont(infoFont);
        
        auto drawShadowText = [&](const QRect& r, int align, const QString& t, const QColor& c) {
            p.setPen(QColor(0, 0, 0, 150));
            p.drawText(r.translated(1, 1), align, t);
            p.setPen(c);
            p.drawText(r, align, t);
        };

        drawShadowText(QRect(12, 36, 160, 22), Qt::AlignLeft | Qt::AlignVCenter, scoreText, QColor(180, 220, 180));
        drawShadowText(QRect(170, 36, 140, 22), Qt::AlignLeft | Qt::AlignVCenter, highScoreText, QColor(205, 190, 235));
        
        const int coinX = width() / 2 - 80;
        drawShadowText(QRect(coinX, 36, 120, 22), Qt::AlignLeft | Qt::AlignVCenter, "Coins: ", QColor(200, 200, 200));
        
        if (coins > m_prevCoins) {
            m_coinPopTimer.start();
            m_prevCoins = coins;
        }

        for (int i = 0; i < 3; ++i) {
            const QColor coinColor = (i < coins) ? QColor(255, 210, 40) : QColor(80, 80, 80);
            p.setBrush(coinColor);
            p.setPen(QPen(coinColor.darker(150), 1));
            
            int r = 6;
            if (i == coins - 1 && m_coinPopTimer.isValid() && m_coinPopTimer.elapsed() < 300) {
                float progress = m_coinPopTimer.elapsed() / 300.0f;
                r = 6 + (int)(std::sin(progress * 3.14159f) * 4.0f);
            }
            p.drawEllipse(QPoint(coinX + 64 + i * 20, 47), r, r);
            
            if (i < coins) {
                p.setPen(QColor(0, 0, 0, 150));
                QFont f = p.font(); f.setPointSize(8); f.setBold(true); p.setFont(f);
                p.drawText(QRect(coinX + 64 + i * 20 - r, 47 - r, r*2, r*2), Qt::AlignCenter, "✓");
                p.setFont(infoFont);
            }
        }
        
        drawShadowText(QRect(coinX + 64 + 3 * 20, 36, 100, 22), Qt::AlignLeft | Qt::AlignVCenter, "(3 to unlock)", QColor(150, 150, 150));

        const int timeRemaining = m_game->timeRemaining();
        QString timePrefix = "";
        QColor timeColor = QColor(200, 220, 200);
        bool timePulsing = false;
        
        if (timeRemaining <= 10) {
            timePrefix = "⏰ ";
            timeColor = QColor(240, 80, 80);
            timePulsing = true;
            QFont boldFont = infoFont; boldFont.setBold(true); p.setFont(boldFont);
        } else if (timeRemaining <= 30) {
            timePrefix = "⚠ ";
            timeColor = QColor(240, 200, 80);
        }
        
        drawShadowText(QRect(width() - 245, 36, 118, 22), Qt::AlignRight | Qt::AlignVCenter, QString("Run: %1").arg(runTimeText), QColor(175, 205, 255));
        
        if (timePulsing) {
            int alpha = 255 - (int)(std::abs(std::sin(m_frameTimer.elapsed() / 150.0f)) * 100);
            timeColor.setAlpha(alpha);
        }
        drawShadowText(QRect(width() - 120, 36, 108, 22), Qt::AlignRight | Qt::AlignVCenter, QString("Time: %1%2").arg(timePrefix, timeText), timeColor);
        p.setFont(infoFont);
        p.restore();
    }

    // Control hints strip
    p.setBrush(QColor(0, 0, 0, 100));
    p.setPen(Qt::NoPen);
    p.drawRect(0, hudHeight() - 20, 240, 20);
    p.setPen(QColor(180, 180, 180, 180));
    QFont hintFont = p.font(); hintFont.setPointSize(8); p.setFont(hintFont);
    p.drawText(QRect(8, hudHeight() - 20, 232, 20), Qt::AlignLeft | Qt::AlignVCenter, "WASD/Arrows: Move  •  Esc: Pause");

    const Level*  level  = m_game->level();
    const Player* player = m_game->player();
    if (!level || !player) return;
    p.save();
    p.translate(0, hudHeight());

    const int mapPixelW = level->getWidth()  * TILE_SIZE;
    const int mapPixelH = level->getHeight() * TILE_SIZE;
    const int viewPixelW = width();
    const int viewPixelH = height() - hudHeight();

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

    auto drawSheetTile = [&p](const QPixmap& sheet, int tileIndex, const QRect& targetRect) -> bool {
        if (sheet.isNull()) {
            return false;
        }
        const int srcTileSize = 32;
        const int cols = sheet.width() / srcTileSize;
        const int rows = sheet.height() / srcTileSize;
        const int count = cols * rows;
        if (count <= 0) {
            return false;
        }
        const int idx = ((tileIndex % count) + count) % count;
        const int sx = (idx % cols) * srcTileSize;
        const int sy = (idx / cols) * srcTileSize;
        p.drawPixmap(targetRect, sheet, QRect(sx, sy, srcTileSize, srcTileSize));
        return true;
    };
    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            const int screenX = x * TILE_SIZE - camX;
            const int screenY = y * TILE_SIZE - camY;
            const QRect cell(screenX, screenY, TILE_SIZE, TILE_SIZE);
            const CellType ct = static_cast<CellType>(level->tileAt(x, y));

            if (ct == CellType::Wall || ct == CellType::HiddenWall) {
                bool drewWall = false;
                if (!m_wallTiles.isEmpty()) {
                    const quint32 roll = tileHash(x, y, 17u) % 100u;
                    int wallIdx = 0;
                    if (roll < 65u) {
                        wallIdx = 0;
                    } else if (roll < 87u) {
                        wallIdx = qMin(1, m_wallTiles.size() - 1);
                    } else if (roll < 93u) {
                        wallIdx = qMin(2, m_wallTiles.size() - 1);
                    } else if (roll < 97u) {
                        wallIdx = qMin(3, m_wallTiles.size() - 1);
                    } else {
                        wallIdx = qMin(4, m_wallTiles.size() - 1);
                    }
                    p.drawPixmap(cell, m_wallTiles.at(wallIdx));
                    drewWall = true;
                }
                if (!drewWall && !drawSheetTile(m_wallTileSheet, (x * 17 + y * 31) % 256, cell)) {
                    static const QColor wallBase[] = {
                        QColor(55, 52, 60),
                        QColor(48, 56, 44),
                        QColor(70, 56, 42),
                    };
                    p.fillRect(cell, wallBase[((x * 7 + y * 3) & 0xFF) % 3]);
                }
                continue;
            }

            bool drewFloor = false;
            if (ct == CellType::OpenedSecretFloor && !m_openedSecretFloorTile.isNull()) {
                p.drawPixmap(cell, m_openedSecretFloorTile);
                drewFloor = true;
            } else if (!m_floorTiles.isEmpty()) {
                const quint32 roll = tileHash(x, y, 29u) % 100u;
                int floorIdx = 0;
                if (ct == CellType::Corridor) {
                    // Corridors must read as a clean path: always stone base.
                    floorIdx = 0;
                } else {
                    if (roll < 60u) {
                        floorIdx = 0;
                    } else if (roll < 78u) {
                        floorIdx = qMin(1, m_floorTiles.size() - 1);
                    } else if (roll < 92u) {
                        floorIdx = qMin(2, m_floorTiles.size() - 1);
                    } else {
                        floorIdx = qMin(3, m_floorTiles.size() - 1);
                    }
                }
                p.drawPixmap(cell, m_floorTiles.at(floorIdx));
                drewFloor = true;
            } else if (!m_floorTileSheet.isNull()) {
                int floorVariant = (x * 13 + y * 19) % 256;
                if (ct == CellType::Corridor) {
                    floorVariant = 0;
                }
                drewFloor = drawSheetTile(m_floorTileSheet, floorVariant, cell);
            }
            if (!drewFloor) {
                const QColor base = tileColour(ct, x, y);
                p.fillRect(cell, base);
                drawTileBorder(p, cell, ct);
            }

            if (ct == CellType::TreasureRoom) {
                if (!m_treasurePedestalTile.isNull()) {
                    p.drawPixmap(cell, m_treasurePedestalTile);
                } else if (!m_propsTileSheet.isNull()) {
                    drawSheetTile(m_propsTileSheet, 0, cell);
                }
            }
        }
    }
    p.save();
    p.translate(-camX, -camY);
    for (GameObject* obj : level->getObjects()) {
        if (!obj) continue;
        const int ox = obj->getX(), oy = obj->getY();
        if (ox >= startX && ox <= endX && oy >= startY && oy <= endY) {
            obj->draw(p, TILE_SIZE);
        }
    }
    for (Enemy* enemy : level->getEnemies()) {
        if (!enemy) continue;
        const int ox = enemy->getX(), oy = enemy->getY();
        if (ox >= startX && ox <= endX && oy >= startY && oy <= endY) {
            enemy->draw(p, TILE_SIZE);
        }
    }
    player->draw(p, TILE_SIZE);
    p.restore();
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

    if (!m_fogPatchOverlay.isNull()) {
        const int fogSize = TILE_SIZE * 2;
        for (int fy = startY; fy <= endY; fy += 3) {
            for (int fx = startX; fx <= endX; fx += 3) {
                const quint32 h = tileHash(fx, fy, 211u);
                if ((h % 100u) < 18u) {
                    const int screenX = fx * TILE_SIZE - camX;
                    const int screenY = fy * TILE_SIZE - camY;
                    p.drawPixmap(QRect(screenX, screenY, fogSize, fogSize),
                                 m_fogPatchOverlay,
                                 QRect(0, 0, m_fogPatchOverlay.width(), m_fogPatchOverlay.height()));
                }
            }
        }
    }

    if (!m_vignetteOverlay.isNull()) {
        p.drawPixmap(QRect(0, 0, viewPixelW, viewPixelH),
                     m_vignetteOverlay,
                     QRect(0, 0, m_vignetteOverlay.width(), m_vignetteOverlay.height()));
    }

    p.restore();

    // Draw hints/status as a final overlay so they always stay above gameplay.
    if (!m_statusText.isEmpty()) {
        float opacity = statusOpacity();
        if (opacity > 0.0f) {
            p.save();
            p.setOpacity(opacity);
            const int overlayY = hudHeight() + 8;
            const int overlayH = 28;
            const QRect panelRect(12, overlayY, width() - 24, overlayH);

            if (m_statusIsAiHint) {
                p.setBrush(QColor(24, 38, 56, 225));
                p.setPen(QPen(QColor(120, 200, 255), 1));
                p.drawRoundedRect(panelRect, 5, 5);

                QFont labelFont = p.font();
                labelFont.setBold(true);
                labelFont.setPointSize(8);
                p.setFont(labelFont);
                p.setPen(QColor(170, 225, 255));
                p.drawText(QRect(panelRect.x() + 8, panelRect.y() + 6, 84, 16),
                           Qt::AlignLeft | Qt::AlignVCenter, "[*] AI Hint");

                QString hintBody = m_statusText;
                if (hintBody.startsWith("AI Hint:", Qt::CaseInsensitive)) {
                    hintBody = hintBody.mid(QString("AI Hint:").size()).trimmed();
                }
                QFont bodyFont = p.font();
                bodyFont.setBold(false);
                p.setFont(bodyFont);
                p.setPen(QColor(225, 235, 245));
                const QRect hintTextRect(panelRect.x() + 96, panelRect.y() + 6, panelRect.width() - 104, 16);
                const QString elidedHint = p.fontMetrics().elidedText(hintBody, Qt::ElideRight, hintTextRect.width());
                p.drawText(hintTextRect, Qt::AlignLeft | Qt::AlignVCenter, elidedHint);
            } else {
                p.setBrush(QColor(34, 28, 22, 220));
                p.setPen(QPen(QColor(210, 180, 120), 1));
                p.drawRoundedRect(panelRect, 5, 5);

                QFont statusFont = p.font();
                statusFont.setItalic(true);
                statusFont.setPointSize(9);
                p.setFont(statusFont);
                p.setPen(QColor(240, 215, 150));
                const QRect statusRect(panelRect.x() + 10, panelRect.y() + 5, panelRect.width() - 20, 18);
                const QString elidedStatus = p.fontMetrics().elidedText(m_statusText, Qt::ElideRight, statusRect.width());
                p.drawText(statusRect, Qt::AlignLeft | Qt::AlignVCenter, elidedStatus);
            }
            p.restore();
        } else {
            m_statusText.clear();
        }
    }

    drawMiniMap(p);

    // Critical state flashes
    if (m_game->timeRemaining() <= 10) {
        int alpha = (int)(std::abs(std::sin(m_frameTimer.elapsed() / 150.0f)) * 100);
        p.setPen(QPen(QColor(255, 0, 0, alpha), 4));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect());
    }

    if (m_objectiveFlashActive && m_objectiveFlashTimer.isValid() && m_objectiveFlashTimer.elapsed() < 400) {
        int alpha = 255 - (m_objectiveFlashTimer.elapsed() * 255 / 400);
        p.setPen(QPen(QColor(255, 215, 0, alpha), 6));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect());
    } else {
        m_objectiveFlashActive = false;
    }
}
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

void GameWindow::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
}

void GameWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_pauseOverlay->setGeometry(rect());
    m_gameOverOverlay->setGeometry(rect());
    layoutHudButtons();
}

void GameWindow::onLevelChanged(int levelIndex)
{
    Q_UNUSED(levelIndex);
    hidePauseOverlay();
    m_gameOverOverlay->hide();
    m_statusText.clear();
    m_statusIsAiHint = false;
    resetExplored();
    resizeToCurrentLevel();
    snapCameraToPlayer();
    injectSprites();
    update();
}

void GameWindow::onClueRevealed(const QString& text)
{
    m_statusText = text;
    m_statusIsAiHint = text.startsWith("AI Hint:", Qt::CaseInsensitive);
    m_statusDurationMs = m_statusIsAiHint ? 6000 : 3500;
    m_statusTimer.start();
    update();
}

void GameWindow::onWallOpened()
{
    m_statusText = "You hear stone grinding... a passage opens!";
    m_statusIsAiHint = false;
    m_statusDurationMs = 3500;
    m_statusTimer.start();
    update();
}

void GameWindow::onTreasureUnlocked()
{
    m_statusText = "Treasure unlocked! Find the treasure room!";
    m_statusIsAiHint = false;
    m_statusDurationMs = 3500;
    m_statusTimer.start();
    m_objectiveFlashActive = true;
    m_objectiveFlashTimer.start();
    update();
}

void GameWindow::onGameOver(bool won, int score)
{
    hidePauseOverlay();
    const bool hasNext = won && (
        m_game->difficulty() == Difficulty::EASY
        || (m_game->currentLevelIndex() + 1 < m_game->levelCount())
    );
    m_gameOverOverlay->setResult(won, score, hasNext);
    m_gameOverOverlay->setGeometry(rect());
    m_gameOverOverlay->show();
    m_gameOverOverlay->raise();
}
void GameWindow::ensureLevelsConfigured()
{
    if (m_levelsConfigured) return;
    const QString levelsDir = findLevelsDir();
    m_game->setLevelFiles(LevelLoader::getAvailableLevels(levelsDir));
    m_levelsConfigured = true;
}

void GameWindow::resizeToCurrentLevel()
{
    setMinimumSize(800, 600);
    resize(1280, 960);
    layoutHudButtons();
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

int GameWindow::hudHeight() const
{
    return qMax(72, height() / 10);
}

void GameWindow::layoutHudButtons()
{
    m_hudRestartBtn->setGeometry(width() - 210, 16, 80, 28);
    m_hudQuitBtn->setGeometry(width() - 110, 16, 80, 28);
}

float GameWindow::statusOpacity() const
{
    if (!m_statusTimer.isValid()) return 0.0f;
    int elapsed = m_statusTimer.elapsed();
    if (elapsed < m_statusDurationMs) return 1.0f;
    int fade = elapsed - m_statusDurationMs;
    if (fade > 500) return 0.0f;
    return 1.0f - (fade / 500.0f);
}

void GameWindow::drawMiniMap(QPainter& p) const
{
    const Level* level = m_game->level();
    const Player* player = m_game->player();
    if (!level || !player || m_explored.isEmpty()) return;

    p.save();
    const int mapW = level->getWidth();
    const int mapH = level->getHeight();
    const int pxMapW = mapW * MINIMAP_CELL;
    const int pxMapH = mapH * MINIMAP_CELL;
    
    // Scale mini-map if it's too large, but typically it fits in MINIMAP_SIZE
    const int actualW = qMin(pxMapW, MINIMAP_SIZE);
    const int actualH = qMin(pxMapH, MINIMAP_SIZE);
    const int panelX = width() - actualW - 20;
    const int panelY = height() - actualH - 20;

    p.translate(panelX, panelY);
    
    p.setBrush(QColor(0, 0, 0, 180));
    p.setPen(QPen(QColor(100, 100, 100, 100), 2));
    p.drawRoundedRect(-4, -4, actualW + 8, actualH + 8, 4, 4);
    
    p.setClipRect(0, 0, actualW, actualH);
    
    for (int y = 0; y < mapH; ++y) {
        for (int x = 0; x < mapW; ++x) {
            if (!m_explored.at(y).at(x)) continue;
            
            CellType ct = static_cast<CellType>(level->tileAt(x, y));
            QColor c;
            switch(ct) {
                case CellType::Wall:
                case CellType::HiddenWall:
                    c = QColor(120, 120, 120); break;
                case CellType::TreasureRoom:
                    c = QColor(255, 215, 0); break;
                default:
                    c = QColor(60, 60, 60); break;
            }
            p.fillRect(x * MINIMAP_CELL, y * MINIMAP_CELL, MINIMAP_CELL, MINIMAP_CELL, c);
        }
    }
    
    for (Coin* coin : level->getCoins()) {
        if (!coin->isCollected() && m_explored.at(coin->getY()).at(coin->getX())) {
            p.fillRect(coin->getX() * MINIMAP_CELL, coin->getY() * MINIMAP_CELL, MINIMAP_CELL, MINIMAP_CELL, QColor(255, 255, 0));
        }
    }
    
    for (Enemy* enemy : level->getEnemies()) {
        if (!enemy->isDead() && m_explored.at(enemy->getY()).at(enemy->getX())) {
            p.fillRect(enemy->getX() * MINIMAP_CELL, enemy->getY() * MINIMAP_CELL, MINIMAP_CELL, MINIMAP_CELL, QColor(255, 50, 50));
        }
    }
    
    if (m_frameTimer.elapsed() % 1000 < 500) {
        p.fillRect(player->getX() * MINIMAP_CELL, player->getY() * MINIMAP_CELL, MINIMAP_CELL, MINIMAP_CELL, QColor(0, 255, 255));
    }
    
    p.restore();
}
