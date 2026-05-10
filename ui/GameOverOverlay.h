#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class GameOverOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit GameOverOverlay(QWidget *parent = nullptr);

    void setResult(bool won, int score, bool hasNextLevel);
    // In multiplayer, call this after the local player presses Restart
    // to show a waiting indicator until the peer also confirms.
    void setMultiplayerWaiting(bool waiting);

signals:
    void nextLevelRequested();
    void restartRequested();
    void mainMenuRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QLabel* m_titleLabel;
    QLabel* m_scoreLabel;
    QLabel* m_waitingLabel;       // shown while waiting for peer restart ACK
    QPushButton* m_nextLevelButton;
    QPushButton* m_restartButton;
    QPushButton* m_mainMenuButton;
};

