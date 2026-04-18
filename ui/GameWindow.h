#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include "Enums.h"
#include "SpriteManager.h"

#include <QElapsedTimer>
#include <QPointF>
#include <QPixmap>
#include <QWidget>

class QTimer;
class Game;
class GameOverOverlay;
class PauseOverlay;

class GameWindow : public QWidget
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget *parent = nullptr);

    void startNewGame(Difficulty difficulty);
    bool loadSavedGame(const QString& filepath);

signals:
    void quitToMainMenuRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onClueRevealed(const QString& text);
    void onWallOpened();
    void onTreasureUnlocked();
    void onGameOver(bool won, int score);

private:
    void ensureLevelsConfigured();
    void resizeToCurrentLevel();
    void showPauseOverlay();
    void hidePauseOverlay();
    void snapCameraToPlayer();
    void loadAssets();
    void injectSprites();
    void resetExplored();

    Game*             m_game;
    PauseOverlay*     m_pauseOverlay;
    GameOverOverlay*  m_gameOverOverlay;
    QTimer*           m_renderTimer;

    QPointF           m_cameraPx;
    QVector<QVector<bool>> m_explored;
    SpriteManager     m_spriteManager;
    QElapsedTimer     m_frameTimer;
    float             m_deltaMs;
    QVector<QPixmap>  m_floorTiles;
    QVector<QPixmap>  m_wallTiles;
    QPixmap           m_treasurePedestalTile;
    QPixmap           m_pressurePlateTile;
    QPixmap           m_vignetteOverlay;
    QPixmap           m_fogPatchOverlay;
    QPixmap           m_floorTileSheet;
    QPixmap           m_wallTileSheet;
    QPixmap           m_propsTileSheet;

    QString  m_statusText;
    bool     m_levelsConfigured;
    bool     m_assetsLoaded;
    bool     m_statusIsAiHint;

    static constexpr int    HUD_HEIGHT             = 96;
    static constexpr int    TILE_SIZE               = 32;
    static constexpr int    VIEWPORT_WIDTH          = 800;
    static constexpr int    VIEWPORT_HEIGHT         = 600;
    static constexpr qreal  CAMERA_LERP             = 0.18;
    static constexpr int    VISIBILITY_RADIUS_TILES = 6;
};

#endif // GAMEWINDOW_H
