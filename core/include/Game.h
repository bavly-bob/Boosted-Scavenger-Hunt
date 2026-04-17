#pragma once

#include "Enums.h"

#include <QObject>
#include <QPoint>
#include <QString>
#include <QStringList>

#include <memory>

class ClueManager;
class Level;
class Player;
class QTimer;

class Game : public QObject {
    Q_OBJECT

    GameState m_state;
    Difficulty m_difficulty;

    int m_score;
    int m_timeRemaining;
    int m_currentLevelIndex;

    QStringList m_levelFiles;
    std::unique_ptr<Level> m_currentLevel;
    std::unique_ptr<Player> m_player;
    std::unique_ptr<ClueManager> m_clueManager;
    QTimer* m_timer;

public:
    explicit Game(QObject* parent = nullptr);
    ~Game() override;

    void setLevelFiles(const QStringList& levelFiles);
    int levelCount() const;

    void startNewGame(Difficulty diff);
    void startLevel(int levelIndex, Difficulty diff);
    void nextLevel();
    void restartLevel();

    void handleInput(Direction dir);
    void pause();
    void resume();

    GameState state() const;
    Difficulty difficulty() const;
    int score() const;
    int timeRemaining() const;
    int currentLevelIndex() const;
    QString currentLevelName() const;
    int coinsCollected() const;
    QPoint playerPosition() const;

    const Level* level() const;
    const Player* player() const;

signals:
    void gameUpdated();
    void clueRevealed(const QString& text);
    void coinCollected(int total);
    void wallOpened();
    void treasureUnlocked();
    void gameOver(bool won, int score);
    void timerTick(int secondsLeft);

private slots:
    void onTick();
};
