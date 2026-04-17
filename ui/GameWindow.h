#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include "Enums.h"

#include <QPointF>
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

signals:
    void quitToMainMenuRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
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

    Game* m_game;
    PauseOverlay* m_pauseOverlay;
    GameOverOverlay* m_gameOverOverlay;
    QTimer* m_renderTimer;

    QPointF m_cameraPx;

    QString m_statusText;
    bool m_levelsConfigured;

    static constexpr int HUD_HEIGHT = 72;
    static constexpr int TILE_SIZE = 32;
    static constexpr int VIEWPORT_WIDTH = 800;
    static constexpr int VIEWPORT_HEIGHT = 600;
    static constexpr qreal CAMERA_LERP = 0.18;
};

#endif // GAMEWINDOW_H

