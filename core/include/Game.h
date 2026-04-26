#pragma once

#include "DifficultyConfig.h"
#include "Enums.h"

#include <QObject>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QSoundEffect>

#include <memory>

class ClueManager;
class Level;
class Player;
class QTimer;
class AIHelper;

class Game : public QObject {
    Q_OBJECT

    GameState m_state;
    Difficulty m_difficulty;

    int m_score;
    int m_highScore;
    int m_timeRemaining;
    int m_runTimeElapsed;
    int m_levelTimeElapsed;
    int m_currentLevelIndex;
    quint32 m_runSeedBase;
    int m_runIndex;
    int m_lootRoomsSpawned;
    QString m_activeDifficultyProfileId;
    ProceduralGenerationRules m_activeGenerationRules;
    DifficultyConfig m_difficultyConfig;

    QStringList m_levelFiles;
    std::unique_ptr<Level> m_currentLevel;
    std::unique_ptr<Player> m_player;
    std::unique_ptr<ClueManager> m_clueManager;
    std::unique_ptr<AIHelper> m_aiHelper;
    QTimer* m_timer;

    // Sound effects
    QSoundEffect m_sfxCoin;
    QSoundEffect m_sfxWallOpen;
    QSoundEffect m_sfxPressurePlate;
    QSoundEffect m_sfxTreasure;
    QSoundEffect m_sfxWin;
    QSoundEffect m_sfxGameOver;
    QSoundEffect m_sfxBlocked;
    QSoundEffect m_sfxClue;
    

    void initSounds();

public:
    explicit Game(QObject* parent = nullptr);
    ~Game() override;

    void setLevelFiles(const QStringList& levelFiles);
    int levelCount() const;

    void startNewGame(Difficulty diff);

    // Multiplayer stubs
    void startMultiplayerMode();
    void hostMultiplayerSession(int port);
    void joinMultiplayerSession(const QString& host, int port);

    void startLevel(int levelIndex, Difficulty diff);
    void nextLevel();
    void restartLevel();
    void saveGame(const QString& filepath);
    bool loadGame(const QString& filepath);
    bool hasSavedGame(const QString& filepath) const;

    void handleInput(Direction dir);
    void pause();
    void resume();

    GameState state() const;
    Difficulty difficulty() const;
    int score() const;
    int highScore() const;
    int timeRemaining() const;
    int runTimeSeconds() const;
    int levelTimeSeconds() const;
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
    void levelChanged(int levelIndex);

private slots:
    void onTick();

private:
    void endRunWithFailure();
};
