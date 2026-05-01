#pragma once

#include "DifficultyConfig.h"
#include "Enums.h"

#include <QObject>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QSoundEffect>

#include <array>
#include <atomic>
#include <memory>
#include <thread>

// Forward-declare Boost.Asio types without pulling in the heavy header here.
namespace boost { namespace asio { class io_context; namespace ip { class tcp; } } }

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
    std::array<std::unique_ptr<Player>, 2> m_players;
    bool m_multiplayerMode;
    std::array<bool, 2> m_playerReachedTreasure;
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

    // ── Multiplayer networking ────────────────────────────────────────────────
    // All socket I/O runs on m_ioThread; Qt signals bridge results back to the
    // main thread safely.  The io_context and socket are heap-allocated so that
    // Boost.Asio headers are not exposed to every translation unit that includes
    // Game.h — only Game.cpp needs to pull in <boost/asio.hpp>.
    struct NetState;                          // defined in Game.cpp (PIMPL)
    std::unique_ptr<NetState> m_net;          // null when not in multiplayer

    int           m_localPlayerIndex = 0;     // 0 = host, 1 = joiner
    std::atomic<uint32_t> m_outSeq{0};        // monotonic per-sender counter (N5)
    uint32_t      m_lastInSeq      = 0;       // last accepted remote sequence

    QTimer* m_netKeepAliveTimer = nullptr;    // pings peer every 5 s

    // Internal helpers (called from I/O thread; signal back to main thread)
    void startNetworkThread();
    void stopNetworkThread();
    void sendGameMessage(const QString& json);
    void scheduleRead();
    void onRawMessage(const std::string& raw);// called on I/O thread → Qt::QueuedConnection

public:
    explicit Game(QObject* parent = nullptr);
    ~Game() override;

    void setLevelFiles(const QStringList& levelFiles);
    int levelCount() const;

    void startNewGame(Difficulty diff);

    // Multiplayer
    void startMultiplayerMode();
    void hostMultiplayerSession(int port);
    void joinMultiplayerSession(const QString& host, int port);

    // Returns which player index this instance owns locally (0 or 1).
    // Used by GameWindow to send only the correct player's input.
    int localPlayerIndex() const { return m_localPlayerIndex; }

    void startLevel(int levelIndex, Difficulty diff);
    void nextLevel();
    void restartLevel();
    void saveGame(const QString& filepath);
    bool loadGame(const QString& filepath);
    bool hasSavedGame(const QString& filepath) const;

    void handleInput(Direction dir);
    void handleInputForPlayer(int playerIndex, Direction dir);
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
    QPoint playerPosition(int playerIndex = 0) const;
    int playerCount() const;
    bool isMultiplayerMode() const;

    const Level* level() const;
    const Player* player(int playerIndex = 0) const;

signals:
    void gameUpdated();
    void clueRevealed(const QString& text);
    void coinCollected(int total);
    void wallOpened();
    void treasureUnlocked();
    void waitingForTeammate(int playerIndex);
    void gameOver(bool won, int score);
    void timerTick(int secondsLeft);
    void levelChanged(int levelIndex);

private slots:
    void onTick();

private:
    void endRunWithFailure();
    void resetTreasureReachState();
    bool isCellOccupiedByOtherPlayer(int playerIndex, int x, int y) const;
    void handlePlayerReachedTreasure(int playerIndex);
};
