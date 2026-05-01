// GameMessage.hpp
// Typed, sequenced message envelope for the ScavengerHunt multiplayer protocol.
// All network payloads pass through this layer so that:
//   - Every message carries a monotonic sequence number (drop duplicates/OOO).
//   - The type enum gives the receiver a fast dispatch path.
//   - encode/decode are the single point where JSON is assembled/parsed.

#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <cstdint>

namespace scavenger::net {

//  Message types 
enum class MsgType : uint8_t {
    Handshake   = 0,  // host → joiner: initial game state
    JoinRequest = 1,  // joiner → host: join intent + version
    Move        = 2,  // either direction: player input
    StateSync   = 3,  // host → joiner: authoritative full-state snapshot
    Ping        = 4,  // either direction: keep-alive
    Pong        = 5,  // echo of Ping
    Unknown     = 255
};

//  Envelope 
struct GameMessage {
    MsgType    type    = MsgType::Unknown;
    uint32_t   seq     = 0;   // monotonically increasing per sender
    QJsonObject payload;

    // Serialise to compact JSON string (ready for protocol::encode())
    [[nodiscard]] QString toJson() const
    {
        QJsonObject root;
        root[QStringLiteral("t")]   = static_cast<int>(type);
        root[QStringLiteral("seq")] = static_cast<int>(seq);
        root[QStringLiteral("p")]   = payload;
        return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }

    // Deserialise from raw JSON string.  Returns false on malformed input.
    [[nodiscard]] static bool fromJson(const QString& raw, GameMessage& out)
    {
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            return false;

        const QJsonObject root = doc.object();
        const int t = root.value(QStringLiteral("t")).toInt(-1);
        if (t < 0 || t > 255)
            return false;

        out.type    = static_cast<MsgType>(static_cast<uint8_t>(t));
        out.seq     = static_cast<uint32_t>(root.value(QStringLiteral("seq")).toInt(0));
        out.payload = root.value(QStringLiteral("p")).toObject();
        return true;
    }
};

// Convenience builders 

inline GameMessage makePing(uint32_t seq)
{
    return { MsgType::Ping, seq, {} };
}

inline GameMessage makePong(uint32_t seq)
{
    return { MsgType::Pong, seq, {} };
}

inline GameMessage makeMove(uint32_t seq, int playerIndex, int direction)
{
    QJsonObject p;
    p[QStringLiteral("pi")]  = playerIndex;
    p[QStringLiteral("dir")] = direction;
    return { MsgType::Move, seq, p };
}

inline GameMessage makeStateSync(uint32_t seq,
                                  int levelIndex,
                                  int difficulty,
                                  int score,
                                  int highScore,
                                  qint64 seedBase,
                                  int runIndex,
                                  int lootRooms,
                                  int runTime,
                                  int levelTime,
                                  int timeRemaining,
                                  int sharedCoins,
                                  int gameState,
                                  int p0x,
                                  int p0y,
                                  int p1x,
                                  int p1y,
                                  bool p0ReachedTreasure,
                                  bool p1ReachedTreasure)
{
    QJsonObject p;
    p[QStringLiteral("levelIndex")]      = levelIndex;
    p[QStringLiteral("difficulty")]      = difficulty;
    p[QStringLiteral("score")]           = score;
    p[QStringLiteral("highScore")]       = highScore;
    p[QStringLiteral("seedBase")]        = seedBase;
    p[QStringLiteral("runIndex")]        = runIndex;
    p[QStringLiteral("lootRoomsSpawned")]= lootRooms;
    p[QStringLiteral("runTimeSeconds")]  = runTime;
    p[QStringLiteral("levelTimeSeconds")]= levelTime;
    p[QStringLiteral("timeRemaining")]   = timeRemaining;
    p[QStringLiteral("sharedCoins")]     = sharedCoins;
    p[QStringLiteral("gameState")]       = gameState;
    p[QStringLiteral("p0x")]             = p0x;
    p[QStringLiteral("p0y")]             = p0y;
    p[QStringLiteral("p1x")]             = p1x;
    p[QStringLiteral("p1y")]             = p1y;
    p[QStringLiteral("p0ReachedTreasure")] = p0ReachedTreasure;
    p[QStringLiteral("p1ReachedTreasure")] = p1ReachedTreasure;
    return { MsgType::StateSync, seq, p };
}

} // namespace scavenger::net
