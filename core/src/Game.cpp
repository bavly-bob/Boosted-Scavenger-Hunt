#include "Game.h"

#include "ClueManager.h"
#include "Enemy.h"
#include "Level.h"
#include "LevelLoader.h"
#include "Player.h"
#include "TreasureRoom.h"

#include "AIHelper.h"
#include "InteractionResult.h"
#include "protocol.hpp"
#include "GameMessage.hpp"

#include <boost/asio.hpp>
#include <QMetaObject>

#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QDir>
#include <QCoreApplication>
#include <QUrl>
#include <QDebug>

#include <chrono>
#include <thread>
#include <vector>

// NetState PIMPL
// Isolates all Boost.Asio types so only Game.cpp pulls in <boost/asio.hpp>.
// io_context runs on ioThread; results are posted back via QMetaObject.
struct Game::NetState {
    boost::asio::io_context io;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work{ io.get_executor() };
    std::shared_ptr<boost::asio::ip::tcp::socket> socket;
    std::thread ioThread;
    std::array<char, 4> headerBuf{};
    std::vector<char>   bodyBuf;
};

namespace {
using boost::asio::ip::tcp;
constexpr std::uint32_t kMaxFrameSize = 10u * 1024u * 1024u;
constexpr unsigned short kRoomPortStart = 47001;
constexpr unsigned short kRoomPortEnd   = 47004;
constexpr int kJoinMaxAttempts = 120;


QString findDifficultyConfigPath()
{
    const QStringList candidates = {
        QDir::current().filePath("levels/difficulty_configs.json"),
        QDir::current().filePath("../levels/difficulty_configs.json"),
        QDir(QCoreApplication::applicationDirPath()).filePath("levels/difficulty_configs.json"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../levels/difficulty_configs.json"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../../levels/difficulty_configs.json")
    };
    for (const QString& path : candidates) {
        if (QFile::exists(path)) {
            return QFileInfo(path).absoluteFilePath();
        }
    }
    return QString();
}

QPoint findSecondarySpawn(const Level& level, const QPoint& primary)
{
    static const std::array<QPoint, 8> offsets = {
        QPoint(1, 0), QPoint(-1, 0), QPoint(0, 1), QPoint(0, -1),
        QPoint(1, 1), QPoint(-1, 1), QPoint(1, -1), QPoint(-1, -1)
    };

    for (const QPoint& offset : offsets) {
        const QPoint candidate = primary + offset;
        if (level.isInBounds(candidate.x(), candidate.y()) && level.isWalkable(candidate.x(), candidate.y())) {
            return candidate;
        }
    }

    for (int y = 0; y < level.getHeight(); ++y) {
        for (int x = 0; x < level.getWidth(); ++x) {
            if (x == primary.x() && y == primary.y()) {
                continue;
            }
            if (level.isWalkable(x, y)) {
                return QPoint(x, y);
            }
        }
    }

    return primary;
}
}

Game::Game(QObject* parent)
    : QObject(parent),
      m_state(GameState::START),
      m_difficulty(Difficulty::NORMAL),
      m_score(0),
      m_highScore(0),
      m_timeRemaining(0),
      m_runTimeElapsed(0),
      m_levelTimeElapsed(0),
      m_currentLevelIndex(0),
      m_runSeedBase(0),
      m_runIndex(0),
      m_lootRoomsSpawned(0),
      m_sharedCoinsCollected(0),
      m_activeGenerationRules(),
      m_difficultyConfig(DifficultyConfig::defaults()),
      m_multiplayerMode(false),
      m_playerReachedTreasure{false, false},
      m_clueManager(std::make_unique<ClueManager>()),
      m_timer(new QTimer(this)),
      m_aiHelper(std::make_unique<AIHelper>(this))
{
    const QString difficultyConfigPath = findDifficultyConfigPath();
    if (!difficultyConfigPath.isEmpty()) {
        m_difficultyConfig = DifficultyConfig::loadFromJson(difficultyConfigPath);
    }

    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &Game::onTick);
    initSounds();
}

Game::~Game()
{
    stopNetworkThread();
}

void Game::initSounds()
{
    auto setup = [](QSoundEffect& sfx, const QString& path, float volume = 1.0f) {
        sfx.setSource(QUrl::fromLocalFile(path));
        sfx.setVolume(volume);
    };

    const QString base = QCoreApplication::applicationDirPath() + "/assets/sounds/";

    setup(m_sfxCoin,          base + "coin.wav");
    setup(m_sfxWallOpen,      base + "wall_open.wav");
    setup(m_sfxPressurePlate, base + "pressure_plate.wav");
    setup(m_sfxTreasure,      base + "treasure.wav");
    setup(m_sfxWin,           base + "win.wav");
    setup(m_sfxGameOver,      base + "game_over.wav");
    setup(m_sfxBlocked,       base + "blocked.wav");
    setup(m_sfxClue, base + "clue.wav");

    // Sounds are loaded asynchronously; no blocking status check needed here.
}

void Game::setLevelFiles(const QStringList& levelFiles)
{
    m_levelFiles = levelFiles;
}

int Game::levelCount() const
{
    return m_levelFiles.size();
}

void Game::startNewGame(Difficulty diff)
{
    m_multiplayerMode = false;
    resetTreasureReachState();
    m_score = 0;
    m_sharedCoinsCollected = 0;
    m_runTimeElapsed = 0;
    m_levelTimeElapsed = 0;
    m_runSeedBase = QRandomGenerator::global()->generate();
    ++m_runIndex;
    m_lootRoomsSpawned = 0;
    const DifficultyProfile profile = m_difficultyConfig.selectProfile(diff, m_runSeedBase, m_runIndex);
    m_activeDifficultyProfileId = profile.id;
    m_activeGenerationRules = profile.rules;
    m_timeRemaining = qMax(10, m_activeGenerationRules.startingTime);
    startLevel(0, diff);
}

void Game::prepareMultiplayerRun()
{
    m_multiplayerMode = true;
    resetTreasureReachState();
    m_state = GameState::START;
    m_score = 0;
    m_sharedCoinsCollected = 0;
    m_runTimeElapsed = 0;
    m_levelTimeElapsed = 0;
    m_currentLevelIndex = 0;
    m_runSeedBase = QRandomGenerator::global()->generate();
    ++m_runIndex;
    m_lootRoomsSpawned = 0;

    const DifficultyProfile profile = m_difficultyConfig.selectProfile(Difficulty::HARD, m_runSeedBase, m_runIndex);
    m_activeDifficultyProfileId = profile.id;
    m_activeGenerationRules = profile.rules;
    m_timeRemaining = qMax(10, m_activeGenerationRules.startingTime);

    m_currentLevel.reset();
    m_players[0].reset();
    m_players[1].reset();
    m_timer->stop();
}

void Game::startMultiplayerMode()
{
    prepareMultiplayerRun();
    emit clueRevealed("Multiplayer co-op: waiting for both players before the run starts.");
    emit gameUpdated();
}

void Game::hostMultiplayerSession(int port)
{
    if (port < 0 || port > 65535) {
        emit clueRevealed("Invalid port.");
        return;
    }

    prepareMultiplayerRun();
    stopNetworkThread();
    m_localPlayerIndex = 0;
    m_lastInSeq        = 0;
    m_net = std::make_unique<NetState>();

    auto acceptor = std::make_shared<tcp::acceptor>(m_net->io);
    boost::system::error_code openEc;
    acceptor->open(tcp::v4(), openEc);
    if (openEc) {
        emit clueRevealed("Host open failed: " + QString::fromStdString(openEc.message()));
        m_net.reset();
        return;
    }

    boost::system::error_code optionEc;
    acceptor->set_option(boost::asio::socket_base::reuse_address(true), optionEc);
    if (optionEc) {
        emit clueRevealed("Host socket option failed: " + QString::fromStdString(optionEc.message()));
        m_net.reset();
        return;
    }

    unsigned short selectedPort = 0;
    boost::system::error_code bindEc;
    boost::system::error_code listenEc;
    auto tryBindPort = [&](unsigned short candidatePort) -> bool {
        bindEc.clear();
        listenEc.clear();
        acceptor->bind(tcp::endpoint(tcp::v4(), candidatePort), bindEc);
        if (bindEc) {
            return false;
        }
        acceptor->listen(boost::asio::socket_base::max_listen_connections, listenEc);
        if (listenEc) {
            return false;
        }
        selectedPort = candidatePort;
        return true;
    };

    if (port == 0) {
        for (unsigned short candidate = kRoomPortStart; candidate <= kRoomPortEnd; ++candidate) {
            if (tryBindPort(candidate)) {
                break;
            }
        }
        if (selectedPort == 0) {
            const QString reason = bindEc ? QString::fromStdString(bindEc.message())
                                          : QString::fromStdString(listenEc.message());
            emit clueRevealed("No available host port in range 47001-47004. Last error: " + reason);
            m_net.reset();
            return;
        }
    } else {
        const unsigned short requestedPort = static_cast<unsigned short>(port);
        if (!tryBindPort(requestedPort)) {
            const QString reason = bindEc ? QString::fromStdString(bindEc.message())
                                          : QString::fromStdString(listenEc.message());
            emit clueRevealed(QStringLiteral("Host bind failed on port %1: %2")
                                  .arg(requestedPort)
                                  .arg(reason));
            m_net.reset();
            return;
        }
    }

    emit clueRevealed(QStringLiteral("Hosting on port %1. Waiting for peer...").arg(selectedPort));

    auto sockPtr = std::make_shared<tcp::socket>(m_net->io);
    acceptor->async_accept(*sockPtr,
        [this, acceptor, sockPtr](const boost::system::error_code& ec) {
            acceptor->close();
            if (ec) {
                QMetaObject::invokeMethod(this, [this, m = ec.message()]() {
                    emit clueRevealed("Accept failed: " + QString::fromStdString(m));
                }, Qt::QueuedConnection);
                return;
            }

            using namespace scavenger::net;
            std::array<char, 4> header{};
            boost::system::error_code readEc;
            boost::asio::read(*sockPtr, boost::asio::buffer(header), readEc);
            if (readEc) {
                QMetaObject::invokeMethod(this, [this, m = readEc.message()]() {
                    emit clueRevealed("Join handshake read failed: " + QString::fromStdString(m));
                }, Qt::QueuedConnection);
                return;
            }

            const uint32_t len = netwatch::protocol::decodeHeader(header.data());
            if (len == 0 || len > kMaxFrameSize) {
                QMetaObject::invokeMethod(this, [this]() {
                    emit clueRevealed("Join handshake failed: invalid frame length.");
                }, Qt::QueuedConnection);
                return;
            }

            std::string body(len, '\0');
            boost::asio::read(*sockPtr, boost::asio::buffer(body), readEc);
            if (readEc) {
                QMetaObject::invokeMethod(this, [this, m = readEc.message()]() {
                    emit clueRevealed("Join handshake body read failed: " + QString::fromStdString(m));
                }, Qt::QueuedConnection);
                return;
            }

            GameMessage joinMsg;
            if (!GameMessage::fromJson(QString::fromStdString(body), joinMsg)
                || joinMsg.type != MsgType::JoinRequest) {
                QMetaObject::invokeMethod(this, [this]() {
                    emit clueRevealed("Join handshake failed: expected JoinRequest.");
                }, Qt::QueuedConnection);
                return;
            }

            // ── Session-full guard ─────────────────────────────────────────
            // Reject any second joiner while a peer is already connected.
            if (m_net->socket && m_net->socket->is_open()) {
                using namespace scavenger::net;
                boost::system::error_code ignored;
                const std::string rej = netwatch::protocol::encode(
                    makeSessionRejected(++m_outSeq, QStringLiteral("Session full")).toJson().toStdString());
                boost::asio::write(*sockPtr, boost::asio::buffer(rej), ignored);
                sockPtr->shutdown(tcp::socket::shutdown_both, ignored);
                sockPtr->close(ignored);
                // Keep the acceptor open so a future attempt can succeed
                QMetaObject::invokeMethod(this, [this]() {
                    emit clueRevealed("A second joiner was rejected (session full).");
                }, Qt::QueuedConnection);
                return;
            }

            m_net->socket = sockPtr;

            const std::string peerAddr = [&]() -> std::string {
                boost::system::error_code ignored;
                return sockPtr->remote_endpoint(ignored).address().to_string();
            }();

            QMetaObject::invokeMethod(this, [this, peerAddr]() {
                emit clueRevealed("Peer joined from " + QString::fromStdString(peerAddr) + ". Loading shared run...");
                // Transition to LOADING before startLevel so the joiner knows
                // it should wait for ReadyAck round-trip before sending moves.
                setSessionState(SessionState::LOADING);
                startLevel(0, Difficulty::HARD);
                emit clueRevealed("Multiplayer co-op: shared coins, shared deaths, synchronized level progression.");
                if (!m_netKeepAliveTimer) {
                    m_netKeepAliveTimer = new QTimer(this);
                    m_netKeepAliveTimer->setInterval(5000);
                    connect(m_netKeepAliveTimer, &QTimer::timeout, this, [this]() {
                        using namespace scavenger::net;
                        sendGameMessage(makePing(++m_outSeq).toJson());
                    });
                }
                m_netKeepAliveTimer->start();
            }, Qt::QueuedConnection);

            scheduleRead();
        });

    startNetworkThread();
}

void Game::joinMultiplayerSession(const QString& host, int port)
{
    if (host.trimmed().isEmpty() || port < 0 || port > 65535) {
        emit clueRevealed("Invalid host or port.");
        return;
    }

    stopNetworkThread();
    m_localPlayerIndex = 1;
    m_lastInSeq        = 0;
    m_net = std::make_unique<NetState>();

    std::vector<unsigned short> candidatePorts;
    if (port == 0) {
        for (unsigned short candidate = kRoomPortStart; candidate <= kRoomPortEnd; ++candidate) {
            candidatePorts.push_back(candidate);
        }
    } else {
        candidatePorts.push_back(static_cast<unsigned short>(port));
    }

    if (candidatePorts.empty()) {
        emit clueRevealed("No candidate ports to scan.");
        m_net.reset();
        return;
    }

    if (port == 0) {
        emit clueRevealed(QStringLiteral("Joining %1. Scanning ports 47001-47004...").arg(host.trimmed()));
    } else {
        emit clueRevealed(QStringLiteral("Joining %1:%2...").arg(host.trimmed()).arg(port));
    }

    const std::string hostStr = host.trimmed().toStdString();
    auto ports = std::make_shared<std::vector<unsigned short>>(std::move(candidatePorts));
    auto candidateIndex = std::make_shared<std::size_t>(0);
    auto attempts = std::make_shared<int>(0);
    auto timer = std::make_shared<boost::asio::steady_timer>(m_net->io);
    auto tryConnect = std::make_shared<std::function<void()>>();

    *tryConnect = [this, hostStr, ports, candidateIndex, attempts, timer, tryConnect]() {
        if (!m_net || ports->empty()) {
            return;
        }

        if (++(*attempts) > kJoinMaxAttempts) {
            QMetaObject::invokeMethod(this, [this]() {
                emit clueRevealed("Failed to find host in scanned ports.");
            }, Qt::QueuedConnection);
            return;
        }

        const unsigned short targetPort = ports->at(*candidateIndex);
        *candidateIndex = (*candidateIndex + 1) % ports->size();

        auto scheduleRetry = [this, attempts, timer, tryConnect]() {
            if (*attempts > kJoinMaxAttempts) {
                QMetaObject::invokeMethod(this, [this]() {
                    emit clueRevealed("Failed to find host in scanned ports.");
                }, Qt::QueuedConnection);
                return;
            }

            timer->expires_after(std::chrono::milliseconds(500));
            timer->async_wait([tryConnect](const boost::system::error_code& tec) {
                if (!tec) {
                    (*tryConnect)();
                }
            });
        };

        auto resolver = std::make_shared<tcp::resolver>(m_net->io);
        resolver->async_resolve(hostStr, std::to_string(targetPort),
            [this, resolver, targetPort, scheduleRetry](const boost::system::error_code& resolveEc,
                                                         tcp::resolver::results_type endpoints) mutable {
                if (resolveEc) {
                    scheduleRetry();
                    return;
                }

                auto sock = std::make_shared<tcp::socket>(m_net->io);
                boost::asio::async_connect(*sock, endpoints,
                    [this, sock, targetPort, scheduleRetry](const boost::system::error_code& connectEc,
                                                            const tcp::endpoint&) mutable {
                        if (connectEc) {
                            scheduleRetry();
                            return;
                        }

                        using namespace scavenger::net;
                        GameMessage req;
                        req.type = MsgType::JoinRequest;
                        req.seq  = ++m_outSeq;
                        req.payload[QStringLiteral("version")] = 1;

                        const std::string reqFrame = netwatch::protocol::encode(req.toJson().toStdString());
                        boost::system::error_code ioEc;
                        boost::asio::write(*sock, boost::asio::buffer(reqFrame), ioEc);
                        if (ioEc) {
                            scheduleRetry();
                            return;
                        }

                        QJsonObject statePayload;
                        bool gotStateSync = false;
                        for (int framesRead = 0; framesRead < 8; ++framesRead) {
                            std::array<char, 4> hdr{};
                            boost::asio::read(*sock, boost::asio::buffer(hdr), ioEc);
                            if (ioEc) {
                                break;
                            }

                            const uint32_t len = netwatch::protocol::decodeHeader(hdr.data());
                            if (len == 0 || len > kMaxFrameSize) {
                                break;
                            }

                            std::string body(len, '\0');
                            boost::asio::read(*sock, boost::asio::buffer(body), ioEc);
                            if (ioEc) {
                                break;
                            }

                            GameMessage msg;
                            if (!GameMessage::fromJson(QString::fromStdString(body), msg)) {
                                continue;
                            }
                            if (msg.type == MsgType::SessionRejected) {
                                // Host explicitly rejected us — do not retry
                                const QString reason = msg.payload.value(
                                    QStringLiteral("reason")).toString(QStringLiteral("Unknown"));
                                QMetaObject::invokeMethod(this, [this, reason]() {
                                    emit sessionRejected(reason);
                                    emit clueRevealed("Session rejected by host: " + reason);
                                }, Qt::QueuedConnection);
                                return;
                            }
                            if (msg.type == MsgType::StateSync) {
                                statePayload = msg.payload;
                                gotStateSync = true;
                                break;
                            }
                            if (msg.type == MsgType::Ping) {
                                const std::string pongFrame = netwatch::protocol::encode(makePong(msg.seq).toJson().toStdString());
                                boost::asio::write(*sock, boost::asio::buffer(pongFrame), ioEc);
                                if (ioEc) {
                                    break;
                                }
                            }
                        }

                        if (!gotStateSync) {
                            scheduleRetry();
                            return;
                        }

                        m_net->socket = sock;
                        QMetaObject::invokeMethod(this, [this, statePayload, targetPort]() {
                            applyAuthoritativeState(statePayload, true);
                            emit clueRevealed(QStringLiteral("Connected on port %1.").arg(targetPort));
                            if (!m_netKeepAliveTimer) {
                                m_netKeepAliveTimer = new QTimer(this);
                                m_netKeepAliveTimer->setInterval(5000);
                                connect(m_netKeepAliveTimer, &QTimer::timeout, this, [this]() {
                                    using namespace scavenger::net;
                                    sendGameMessage(makePing(++m_outSeq).toJson());
                                });
                            }
                            m_netKeepAliveTimer->start();
                        }, Qt::QueuedConnection);

                        scheduleRead();
                    });
            });
    };

    (*tryConnect)();
    startNetworkThread();
}

void Game::startNetworkThread()
{
    if (!m_net) return;
    m_net->ioThread = std::thread([this]() { m_net->io.run(); });
}

void Game::stopNetworkThread()
{
    if (!m_net) return;
    if (m_net->socket && m_net->socket->is_open()) {
        boost::system::error_code ec;
        m_net->socket->shutdown(tcp::socket::shutdown_both, ec);
        m_net->socket->close(ec);
    }
    m_net->work.reset(); // let io.run() exit
    if (m_net->ioThread.joinable())
        m_net->ioThread.join();
    m_net.reset();
    if (m_netKeepAliveTimer)
        m_netKeepAliveTimer->stop();
}

void Game::sendGameMessage(const QString& json)
{
    if (!m_net || !m_net->socket || !m_net->socket->is_open()) return;
    const std::string frame = netwatch::protocol::encode(json.toStdString());
    auto buf  = std::make_shared<std::string>(frame);
    auto sock = m_net->socket;
    boost::asio::post(m_net->io, [sock, buf]() {
        boost::asio::async_write(*sock, boost::asio::buffer(*buf),
            [buf](const boost::system::error_code&, std::size_t){});
    });
}

void Game::scheduleRead()
{
    if (!m_net || !m_net->socket) return;
    boost::asio::async_read(*m_net->socket,
        boost::asio::buffer(m_net->headerBuf),
        [this](const boost::system::error_code& ec, std::size_t)
        {
            if (ec) {
                QMetaObject::invokeMethod(this, [this]() {
                    using namespace scavenger::net;
                    // Notify the remaining client (if host) then tear down cleanly
                    if (m_localPlayerIndex == 0 && isAuthoritativeMultiplayerPeer()) {
                        sendGameMessage(makePeerDisconnected(++m_outSeq,
                            QStringLiteral("Peer connection lost")).toJson());
                    }
                    setSessionState(m_localPlayerIndex == 0
                        ? SessionState::SESSION_CLOSED
                        : SessionState::DISCONNECTED);
                    emit peerDisconnected(QStringLiteral("Connection to peer lost."));
                    stopNetworkThread();
                }, Qt::QueuedConnection);
                return;
            }
            const uint32_t len = netwatch::protocol::decodeHeader(m_net->headerBuf.data());
            if (len == 0 || len > kMaxFrameSize) { scheduleRead(); return; }

            m_net->bodyBuf.resize(len);
            boost::asio::async_read(*m_net->socket,
                boost::asio::buffer(m_net->bodyBuf),
                [this](const boost::system::error_code& ec2, std::size_t)
                {
                    if (ec2) {
                        QMetaObject::invokeMethod(this, [this]() {
                            emit clueRevealed("Peer disconnected.");
                        }, Qt::QueuedConnection);
                        return;
                    }
                    std::string raw(m_net->bodyBuf.data(), m_net->bodyBuf.size());
                    onRawMessage(raw);
                    scheduleRead(); // loop
                });
        });
}

void Game::onRawMessage(const std::string& raw)
{
    using namespace scavenger::net;
    GameMessage msg;
    if (!GameMessage::fromJson(QString::fromStdString(raw), msg)) return;

    // Sequence dedup — drop duplicate or out-of-order (N5)
    if (msg.type != MsgType::Ping && msg.type != MsgType::Pong) {
        if (msg.seq != 0 && msg.seq <= m_lastInSeq) return;
        m_lastInSeq = msg.seq;
    }

    switch (msg.type) {
    case MsgType::Ping:
        sendGameMessage(makePong(msg.seq).toJson());
        break;
    case MsgType::Pong:
        break; // confirms peer is alive; could update last-seen timestamp
    case MsgType::Move: {
        const int pi  = msg.payload.value(QStringLiteral("pi")).toInt(-1);
        const int dir = msg.payload.value(QStringLiteral("dir")).toInt(0);
        if (pi < 0 || pi > 1) break;
        const Direction d = static_cast<Direction>(dir);
        // Dispatch to main thread — handleInputForPlayer is NOT thread-safe
        QMetaObject::invokeMethod(this, [this, pi, d]() {
            handleInputForPlayer(pi, d);
        }, Qt::QueuedConnection);
        break;
    }
    case MsgType::StateSync:
        QMetaObject::invokeMethod(this, [this, payload = msg.payload]() {
            applyAuthoritativeState(payload, false);
        }, Qt::QueuedConnection);
        break;

    // ── Session management messages ──────────────────────────────────────
    case MsgType::RestartReady: {
        // Only the host processes this — the joiner never receives it
        if (m_localPlayerIndex != 0) break;
        const int fromPlayer = msg.payload.value(QStringLiteral("pi")).toInt(1);
        QMetaObject::invokeMethod(this, [this, fromPlayer]() {
            onRestartReadyReceived(fromPlayer);
        }, Qt::QueuedConnection);
        break;
    }
    case MsgType::ReadyAck: {
        // Only the host cares: joiner has finished loading.
        if (m_localPlayerIndex != 0) break;
        QMetaObject::invokeMethod(this, [this]() {
            if (m_sessionState == SessionState::LOADING ||
                m_sessionState == SessionState::RESTARTING) {
                // Both sides are loaded — open gameplay
                setSessionState(SessionState::PLAYING);
            }
        }, Qt::QueuedConnection);
        break;
    }
    case MsgType::SessionStateMsg: {
        // Joiner mirrors the host's authoritative state
        const int ssVal = msg.payload.value(QStringLiteral("ss")).toInt(-1);
        if (ssVal < 0) break;
        const SessionState ss = static_cast<SessionState>(ssVal);
        QMetaObject::invokeMethod(this, [this, ss, ssVal]() {
            m_sessionState = ss;
            emit sessionStateChanged(ssVal); // emit as int
            if (ss == SessionState::SESSION_CLOSED ||
                ss == SessionState::DISCONNECTED) {
                emit peerDisconnected(QStringLiteral("Host closed the session."));
                stopNetworkThread();
            }
        }, Qt::QueuedConnection);
        break;
    }
    case MsgType::PeerDisconnected: {
        const QString reason = msg.payload.value(
            QStringLiteral("reason")).toString(QStringLiteral("Unknown"));
        QMetaObject::invokeMethod(this, [this, reason]() {
            m_sessionState = SessionState::DISCONNECTED;
            emit sessionStateChanged(static_cast<int>(SessionState::DISCONNECTED));
            emit peerDisconnected(reason);
            stopNetworkThread();
        }, Qt::QueuedConnection);
        break;
    }
    default:
        break;
    }
}

void Game::applySharedCoins(int total)
{
    total = qMax(0, total);
    m_sharedCoinsCollected = total;
    for (auto& maybePlayer : m_players) {
        if (maybePlayer) {
            maybePlayer->setCoinsCollected(total);
        }
    }
}

bool Game::isAuthoritativeMultiplayerPeer() const
{
    return m_multiplayerMode
        && m_localPlayerIndex == 0
        && m_net
        && m_net->socket
        && m_net->socket->is_open();
}

void Game::sendAuthoritativeState()
{
    if (!isAuthoritativeMultiplayerPeer()) {
        return;
    }

    using namespace scavenger::net;

    const QPoint p0 = playerPosition(0);
    const QPoint p1 = playerPosition(1);
    sendGameMessage(makeStateSync(++m_outSeq,
                                  m_currentLevelIndex,
                                  static_cast<int>(m_difficulty),
                                  m_score,
                                  m_highScore,
                                  static_cast<qint64>(m_runSeedBase),
                                  m_runIndex,
                                  m_lootRoomsSpawned,
                                  m_runTimeElapsed,
                                  m_levelTimeElapsed,
                                  m_timeRemaining,
                                  m_sharedCoinsCollected,
                                  static_cast<int>(m_state),
                                  p0.x(),
                                  p0.y(),
                                  p1.x(),
                                  p1.y(),
                                  m_playerReachedTreasure[0],
                                  m_playerReachedTreasure[1],
                                  static_cast<int>(m_sessionState)).toJson());
}

void Game::applyAuthoritativeState(const QJsonObject& payload, bool announceConnection)
{
    const int levelIndex = qMax(0, payload.value(QStringLiteral("levelIndex")).toInt(0));
    const int diffInt    = payload.value(QStringLiteral("difficulty")).toInt(1);
    Difficulty diff = Difficulty::NORMAL;
    if (diffInt == 0) diff = Difficulty::EASY;
    else if (diffInt == 2) diff = Difficulty::HARD;

    quint32 seedBase = static_cast<quint32>(payload.value(QStringLiteral("seedBase")).toDouble(0.0));
    if (seedBase == 0u) {
        seedBase = QRandomGenerator::global()->generate();
    }

    const int runIndex = qMax(1, payload.value(QStringLiteral("runIndex")).toInt(1));
    const int lootRoomsSpawned = qMax(0, payload.value(QStringLiteral("lootRoomsSpawned")).toInt(0));

    const bool needsLevelReload =
        !m_currentLevel
        || !m_players[0]
        || !m_multiplayerMode
        || m_currentLevelIndex != levelIndex
        || m_difficulty != diff
        || m_runSeedBase != seedBase
        || m_runIndex != runIndex;

    // ── Session state from the authoritative host ──────────────────────────────
    const int ssRaw = payload.value(QStringLiteral("sessionState")).toInt(
        static_cast<int>(SessionState::PLAYING));
    const SessionState remoteSession = static_cast<SessionState>(ssRaw);

    // Joiner must only trigger a level reload when the host signals RESTARTING
    // or LOADING.  Any StateSync that arrives while we're still in PLAYING must
    // NOT reload — it is just a positional update.
    const bool hostAuthorisesReload = (remoteSession == SessionState::RESTARTING ||
                                       remoteSession == SessionState::LOADING);

    if (needsLevelReload && !hostAuthorisesReload) {
        // Ignore the reload portion; still apply position/score/timer updates below.
    } else if (needsLevelReload && hostAuthorisesReload) {
        m_multiplayerMode = true;
        m_score = payload.value(QStringLiteral("score")).toInt(0);
        m_highScore = payload.value(QStringLiteral("highScore")).toInt(m_score);
        m_runSeedBase = seedBase;
        m_runIndex = runIndex;
        m_lootRoomsSpawned = lootRoomsSpawned;

        const DifficultyProfile profile = m_difficultyConfig.selectProfile(diff, m_runSeedBase, m_runIndex);
        m_activeDifficultyProfileId = profile.id;
        m_activeGenerationRules     = profile.rules;

        m_timeRemaining = qMax(10, m_activeGenerationRules.startingTime);
        startLevel(levelIndex, diff);
        // startLevel() sends ReadyAck to host (joiner path) and returns
        // — the rest of this function continues to sync timers/positions.
    }

    // Always sync these regardless of whether a reload happened
    m_multiplayerMode = true;
    m_score = payload.value(QStringLiteral("score")).toInt(0);
    m_highScore = payload.value(QStringLiteral("highScore")).toInt(m_score);
    m_runSeedBase = seedBase;
    m_runIndex = runIndex;
    m_lootRoomsSpawned = lootRoomsSpawned;

    const DifficultyProfile profile = m_difficultyConfig.selectProfile(diff, m_runSeedBase, m_runIndex);
    m_activeDifficultyProfileId = profile.id;
    m_activeGenerationRules     = profile.rules;

    m_runTimeElapsed  = qMax(0, payload.value(QStringLiteral("runTimeSeconds")).toInt(0));
    m_levelTimeElapsed= qMax(0, payload.value(QStringLiteral("levelTimeSeconds")).toInt(0));
    m_timeRemaining   = qMax(0, payload.value(QStringLiteral("timeRemaining")).toInt(m_timeRemaining));
    applySharedCoins(payload.value(QStringLiteral("sharedCoins")).toInt(m_sharedCoinsCollected));
    m_playerReachedTreasure[0] = payload.value(QStringLiteral("p0ReachedTreasure")).toBool(false);
    m_playerReachedTreasure[1] = payload.value(QStringLiteral("p1ReachedTreasure")).toBool(false);

    if (m_players[0]) {
        m_players[0]->teleportTo(payload.value(QStringLiteral("p0x")).toInt(m_players[0]->getX()),
                                 payload.value(QStringLiteral("p0y")).toInt(m_players[0]->getY()));
    }
    if (m_players[1]) {
        m_players[1]->teleportTo(payload.value(QStringLiteral("p1x")).toInt(m_players[1]->getX()),
                                 payload.value(QStringLiteral("p1y")).toInt(m_players[1]->getY()));
    }

    // Mirror session state from host
    if (m_sessionState != remoteSession) {
        m_sessionState = remoteSession;
        emit sessionStateChanged(static_cast<int>(remoteSession));
    }

    const int stateValue = payload.value(QStringLiteral("gameState")).toInt(static_cast<int>(GameState::PLAYING));
    const GameState remoteState = (stateValue >= static_cast<int>(GameState::START)
                                   && stateValue <= static_cast<int>(GameState::WIN))
                                      ? static_cast<GameState>(stateValue)
                                      : GameState::PLAYING;

    if (remoteState == GameState::GAME_OVER) {
        m_state = GameState::GAME_OVER;
        m_timer->stop();
        emit timerTick(m_timeRemaining);
        emit gameUpdated();
        emit gameOver(false, m_score);
        return;
    }

    if (remoteState == GameState::WIN) {
        m_state = GameState::WIN;
        m_timer->stop();
        emit timerTick(m_timeRemaining);
        emit gameUpdated();
        emit gameOver(true, m_score);
        return;
    }

    if (remoteState == GameState::PAUSED) {
        m_state = GameState::PAUSED;
        m_timer->stop();
    } else if (remoteState == GameState::PLAYING) {
        m_state = GameState::PLAYING;
        if (!m_timer->isActive()) {
            m_timer->start();
        }
    } else {
        m_state = remoteState;
        m_timer->stop();
    }

    emit timerTick(m_timeRemaining);
    if (announceConnection) {
        emit clueRevealed("Connected to host. You are Player 2.");
    }
    emit gameUpdated();
}

// ─────────────────────────────────────────────────────────────────────────────

void Game::startLevel(int levelIndex, Difficulty diff)
{
    if (levelIndex < 0) {
        return;
    }
    const bool hasStaticLevel = levelIndex < m_levelFiles.size();

    m_timer->stop();

    m_state = GameState::PLAYING;
    m_difficulty = diff;
    m_currentLevelIndex = levelIndex;

    QJsonArray clues;
    ProceduralGenerationContext generationContext;
    generationContext.rules = m_activeGenerationRules;
    generationContext.runSeedBase = m_runSeedBase;
    generationContext.runIndex = m_runIndex;
    generationContext.levelIndex = levelIndex;
    generationContext.lootRoomsSpawnedThisRun = m_lootRoomsSpawned;
    bool spawnedLootRoom = false;
    if (m_difficulty == Difficulty::EASY) {
        if (hasStaticLevel) {
            m_currentLevel.reset(LevelLoader::loadFromJson(m_levelFiles.at(levelIndex), &clues));
        } else {
            quint32 seed = m_runSeedBase;
            seed ^= static_cast<quint32>(levelIndex + 1) * 0x9e3779b9u;
            seed ^= 0x13579bdfu;
            m_currentLevel.reset(LevelLoader::generateProcedural(static_cast<int>(seed),
                                                                 0,
                                                                 generationContext,
                                                                 &clues,
                                                                 &spawnedLootRoom));
        }
    } else {
        quint32 seed = m_runSeedBase;
        seed ^= static_cast<quint32>(levelIndex + 1) * 0x9e3779b9u;
        seed ^= static_cast<quint32>(m_difficulty == Difficulty::HARD ? 2 : 1) * 0x85ebca6bu;
        const QString anchor = hasStaticLevel ? m_levelFiles.at(levelIndex) : QStringLiteral("procedural");
        for (QChar ch : anchor) {
            seed = seed * 33u + static_cast<quint32>(ch.unicode());
        }

        const int proceduralDifficulty = (m_difficulty == Difficulty::HARD) ? 2 : 1;
        m_currentLevel.reset(LevelLoader::generateProcedural(static_cast<int>(seed),
                                                             proceduralDifficulty,
                                                             generationContext,
                                                             &clues,
                                                             &spawnedLootRoom));

        // Fallback path if procedural generation fails for any reason.
        if (!m_currentLevel && hasStaticLevel) {
            m_currentLevel.reset(LevelLoader::loadFromJson(m_levelFiles.at(levelIndex), &clues));
        }
    }
    if (spawnedLootRoom) {
        ++m_lootRoomsSpawned;
    }
    m_clueManager->loadClues(clues);

    if (!m_currentLevel) {
        m_state = GameState::GAME_OVER;
        emit gameOver(false, m_score);
        return;
    }

    const QPoint spawn = m_currentLevel->getSpawn();
    m_players[0] = std::make_unique<Player>(spawn.x(), spawn.y());
    m_players[1].reset();
    if (m_multiplayerMode) {
        const QPoint secondarySpawn = findSecondarySpawn(*m_currentLevel, spawn);
        m_players[1] = std::make_unique<Player>(secondarySpawn.x(), secondarySpawn.y());
    }
    applySharedCoins(m_sharedCoinsCollected);
    resetTreasureReachState();
    m_levelTimeElapsed = 0;
    if (m_timeRemaining <= 0) {
        m_timeRemaining = qMax(10, m_activeGenerationRules.startingTime);
    }

    // Apply the config-driven coin requirement to every player.
    const int coinsNeeded = m_activeGenerationRules.coinsRequired;
    for (auto& p : m_players) {
        if (p) p->setCoinsRequired(coinsNeeded);
    }

    emit levelChanged(m_currentLevelIndex);
    emit timerTick(m_timeRemaining);
    emit gameUpdated();
    m_timer->start();

    // ── ReadyAck handshake ───────────────────────────────────────────────
    if (m_multiplayerMode && m_localPlayerIndex == 0) {
        // Host sends authoritative state; joiner learns the new seed/index and
        // starts its own level. SessionState stays LOADING until ReadyAck arrives.
        sendAuthoritativeState();
    } else if (m_multiplayerMode && m_localPlayerIndex != 0) {
        // Joiner signals it has finished loading
        using namespace scavenger::net;
        sendGameMessage(makeReadyAck(++m_outSeq).toJson());
    }
    // Single-player: nothing to send.
}

void Game::nextLevel()
{
    if (m_difficulty == Difficulty::EASY) {
        startLevel(m_currentLevelIndex + 1, m_difficulty);
        return;
    }

    if (m_levelFiles.isEmpty()) {
        startLevel(m_currentLevelIndex + 1, m_difficulty);
        return;
    }

    const int nextIndex = (m_currentLevelIndex + 1) % m_levelFiles.size();
    startLevel(nextIndex, m_difficulty);
}

void Game::restartLevel()
{
    // ── Single-player: reload immediately (unchanged behaviour)
    if (!m_multiplayerMode) {
        startLevel(m_currentLevelIndex, m_difficulty);
        return;
    }

    // ── Multiplayer: 2-phase commit ──────────────────────────────────
    // Only accept a restart request while we are in the correct waiting state.
    // Spurious calls (e.g. key held down, duplicate UI events) are ignored.
    if (m_sessionState != SessionState::WAITING_FOR_RESTART_CONFIRMATION) {
        return;
    }

    using namespace scavenger::net;
    if (m_localPlayerIndex == 0) {
        // Host counts itself as ready immediately
        onRestartReadyReceived(0);
    } else {
        // Joiner sends its ACK to the host
        sendGameMessage(makeRestartReady(++m_outSeq, m_localPlayerIndex).toJson());
    }
}

void Game::saveGame(const QString& filepath)
{
    QJsonObject root;
    root["levelIndex"] = m_currentLevelIndex;
    root["difficulty"] = static_cast<int>(m_difficulty);
    root["score"] = m_score;
    root["highScore"] = m_highScore;
    root["seedBase"] = static_cast<qint64>(m_runSeedBase);
    root["runIndex"] = m_runIndex;
    root["lootRoomsSpawned"] = m_lootRoomsSpawned;
    root["difficultyProfileId"] = m_activeDifficultyProfileId;
    root["runTimeSeconds"] = m_runTimeElapsed;
    root["levelTimeSeconds"] = m_levelTimeElapsed;
    root["timeRemaining"] = m_timeRemaining;

    QJsonDocument doc(root);
    QFile file(filepath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

bool Game::loadGame(const QString& filepath)
{
    m_multiplayerMode = false;
    resetTreasureReachState();
    m_sharedCoinsCollected = 0;
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return false;
    QJsonObject root = doc.object();

    int levelIndex = root["levelIndex"].toInt(0);
    int diffInt = root["difficulty"].toInt(1);
    m_score = root["score"].toInt(0);
    m_highScore = root["highScore"].toInt(m_score);
    m_runSeedBase = static_cast<quint32>(root["seedBase"].toDouble(0.0));
    m_runIndex = qMax(1, root["runIndex"].toInt(1));
    m_lootRoomsSpawned = qMax(0, root["lootRoomsSpawned"].toInt(0));
    const int savedRunTime = root["runTimeSeconds"].toInt(0);
    const int savedLevelTime = root["levelTimeSeconds"].toInt(0);
    const int savedTimeRemaining = root["timeRemaining"].toInt(-1);
    if (m_runSeedBase == 0u) {
        m_runSeedBase = QRandomGenerator::global()->generate();
    }

    Difficulty diff = Difficulty::NORMAL;
    if (diffInt == 0) diff = Difficulty::EASY;
    else if (diffInt == 2) diff = Difficulty::HARD;

    const DifficultyProfile profile = m_difficultyConfig.selectProfile(diff, m_runSeedBase, m_runIndex);
    m_activeDifficultyProfileId = profile.id;
    m_activeGenerationRules = profile.rules;
    m_timeRemaining = qMax(10, m_activeGenerationRules.startingTime);

    startLevel(levelIndex, diff);
    m_runTimeElapsed = qMax(0, savedRunTime);
    m_levelTimeElapsed = qMax(0, savedLevelTime);
    if (savedTimeRemaining >= 0) {
        m_timeRemaining = qMax(0, savedTimeRemaining);
    } else {
        m_timeRemaining = qMax(0, m_activeGenerationRules.startingTime - m_runTimeElapsed);
    }
    emit timerTick(m_timeRemaining);
    emit gameUpdated();
    return true;
}

bool Game::hasSavedGame(const QString& filepath) const
{
    return QFile::exists(filepath);
}

void Game::handleInput(Direction dir)
{
    handleInputForPlayer(0, dir);
}

void Game::handleInputForPlayer(int playerIndex, Direction dir)
{
    // ── Session gate: block all moves unless the session is in PLAYING state.
    // This prevents Move packets from corrupting state during level loads,
    // restarts, or death screens on either side.
    if (m_multiplayerMode && m_sessionState != SessionState::PLAYING) {
        return;
    }
    if (m_state != GameState::PLAYING || !m_currentLevel || dir == Direction::None) {
        return;
    }
    if (playerIndex < 0 || playerIndex >= static_cast<int>(m_players.size())) {
        return;
    }

    Player* activePlayer = m_players[playerIndex].get();
    if (!activePlayer) {
        return;
    }
    if (m_multiplayerMode && m_playerReachedTreasure[playerIndex]) {
        return;
    }

    const QPoint currentPos(activePlayer->getX(), activePlayer->getY());

    int targetX = currentPos.x();
    int targetY = currentPos.y();
    switch (dir) {
    case Direction::Up:
        targetY -= 1;
        break;
    case Direction::Down:
        targetY += 1;
        break;
    case Direction::Left:
        targetX -= 1;
        break;
    case Direction::Right:
        targetX += 1;
        break;
    case Direction::None:
        break;
    }

    QString blockedReason;
    if (!m_currentLevel->canPlayerEnter(targetX, targetY, *activePlayer, &blockedReason)) {
        m_sfxBlocked.stop();
        m_sfxBlocked.play();
        if (!blockedReason.isEmpty() && blockedReason.contains("locked", Qt::CaseInsensitive)) {
            emit clueRevealed(blockedReason);
            emit gameUpdated();
        }
        return;
    }
    if (isCellOccupiedByOtherPlayer(playerIndex, targetX, targetY)) {
        m_sfxBlocked.stop();
        m_sfxBlocked.play();
        return;
    }

    activePlayer->move(dir, *m_currentLevel);
    const QPoint newPos(activePlayer->getX(), activePlayer->getY());

    // N4: transmit the validated move to the peer so their client applies it
    if (m_multiplayerMode && playerIndex == m_localPlayerIndex && newPos != currentPos) {
        using namespace scavenger::net;
        sendGameMessage(makeMove(++m_outSeq, playerIndex, static_cast<int>(dir)).toJson());
    }

    if (newPos == currentPos) {
        return;
    }

    for (Enemy* enemy : m_currentLevel->getEnemies()) {
        if (!enemy->isDead() && enemy->getX() == newPos.x() && enemy->getY() == newPos.y()) {
            endRunWithFailure();
            return;
        }
    }

    InteractionResult interaction = m_currentLevel->interactAt(newPos.x(), newPos.y(), *activePlayer, *m_clueManager);
    if (interaction.scoreDelta != 0) {
        m_score += interaction.scoreDelta;
        if (m_score > m_highScore) {
            m_highScore = m_score;
        }
    }
    if (interaction.coinCollected) {
        if (m_multiplayerMode) {
            applySharedCoins(activePlayer->getCoinsCollected());
            emit coinCollected(m_sharedCoinsCollected);
        } else {
            emit coinCollected(interaction.coinsCollectedTotal);
        }
        m_sfxCoin.stop();
        m_sfxCoin.play();
    }
    bool clueShownThisStep = false;
    for (const QString& clue : interaction.revealedClues) {
        clueShownThisStep = true;
    m_sfxClue.stop();
    m_sfxClue.play();   
        if (m_aiHelper && m_aiHelper->isEnabled()) {
            m_aiHelper->rephrase(clue, [this](QString transformed) {
                transformed = transformed.trimmed();
                if (!transformed.startsWith("AI Hint:", Qt::CaseInsensitive)) {
                    transformed.prepend("AI Hint: ");
                }
                emit clueRevealed(transformed);
            });
        } else {
            QString fallbackHint = clue.trimmed();
            if (!fallbackHint.startsWith("AI Hint:", Qt::CaseInsensitive)) {
                fallbackHint.prepend("AI Hint: ");
            }
            emit clueRevealed(fallbackHint);
        }
    }
    if (interaction.wallOpened) {
        m_sfxWallOpen.stop();
        m_sfxWallOpen.play();
        emit wallOpened();
    } else if (interaction.triggerActivated && !clueShownThisStep) {
        m_sfxPressurePlate.stop();
        m_sfxPressurePlate.play();
        emit clueRevealed("The pressure plate clicks, but no nearby wall moved.");
    }
    if (interaction.treasureUnlocked && !clueShownThisStep) {
        m_sfxTreasure.play();
        emit treasureUnlocked();
    }
    if (interaction.won) { 
        handlePlayerReachedTreasure(playerIndex);
        return;
    }

    emit gameUpdated();
    // Send a full StateSync only when meaningful state changed (score or coins).
    // Pure movement is handled by the lightweight Move packet; the 1-Hz onTick
    // StateSync corrects any position drift. This removes the biggest per-keypress
    // CPU/network overhead in both single-player and multiplayer.
    if (interaction.scoreDelta != 0 || interaction.coinCollected) {
        sendAuthoritativeState();
    }

}

void Game::pause()
{
    if (m_state != GameState::PLAYING) {
        return;
    }
    m_state = GameState::PAUSED;
    m_timer->stop();
    emit gameUpdated();
    sendAuthoritativeState();
}

void Game::resume()
{
    if (m_state != GameState::PAUSED) {
        return;
    }
    m_state = GameState::PLAYING;
    m_timer->start();
    emit gameUpdated();
    sendAuthoritativeState();
}

GameState Game::state() const
{
    return m_state;
}

Difficulty Game::difficulty() const
{
    return m_difficulty;
}

int Game::score() const
{
    return m_score;
}

int Game::highScore() const
{
    return m_highScore;
}

int Game::timeRemaining() const
{
    return m_timeRemaining;
}

int Game::runTimeSeconds() const
{
    return m_runTimeElapsed;
}

int Game::levelTimeSeconds() const
{
    return m_levelTimeElapsed;
}

int Game::currentLevelIndex() const
{
    return m_currentLevelIndex;
}

QString Game::currentLevelName() const
{
    return m_currentLevel ? m_currentLevel->getName() : QString();
}

int Game::coinsCollected() const
{
    return m_sharedCoinsCollected;
}

QPoint Game::playerPosition(int playerIndex) const
{
    const Player* p = player(playerIndex);
    return p ? QPoint(p->getX(), p->getY()) : QPoint();
}

int Game::playerCount() const
{
    return m_players[1] ? 2 : (m_players[0] ? 1 : 0);
}

int Game::coinsToUnlock() const
{
    return m_activeGenerationRules.coinsRequired;
}

bool Game::isMultiplayerMode() const
{
    return m_multiplayerMode;
}

const Level* Game::level() const
{
    return m_currentLevel.get();
}

const Player* Game::player(int playerIndex) const
{
    if (playerIndex < 0 || playerIndex >= static_cast<int>(m_players.size())) {
        return nullptr;
    }
    return m_players[playerIndex].get();
}

void Game::onTick()
{
    if (m_state != GameState::PLAYING) {
        return;
    }

    // Pre-compute once per tick: whether dual-player enemy targeting is active.
    const bool dualPlayerActive = m_multiplayerMode && m_players[0] && m_players[1];

    if (m_currentLevel) {
        const Player* chaseTarget = m_players[0].get();
        for (Enemy* enemy : m_currentLevel->getEnemies()) {
            enemy->advanceAnimation();
            const Player* target = chaseTarget;
            if (dualPlayerActive) {
                const int d0 = qAbs(enemy->getX() - m_players[0]->getX())
                             + qAbs(enemy->getY() - m_players[0]->getY());
                const int d1 = qAbs(enemy->getX() - m_players[1]->getX())
                             + qAbs(enemy->getY() - m_players[1]->getY());
                target = (d1 < d0) ? static_cast<const Player*>(m_players[1].get())
                                   : static_cast<const Player*>(m_players[0].get());
            }
            if (target) {
                enemy->update(*m_currentLevel, *target);
            }

            if (!enemy->isDead()) {
                for (const auto& maybePlayer : m_players) {
                    if (!maybePlayer) continue;
                    if (enemy->getX() == maybePlayer->getX() &&
                        enemy->getY() == maybePlayer->getY()) {
                        endRunWithFailure();
                        return;
                    }
                }
            }
        }
    }

    const bool advancesSharedClock = !m_multiplayerMode || m_localPlayerIndex == 0;
    if (advancesSharedClock) {
        if (m_timeRemaining > 0) {
            m_timeRemaining -= 1;
        }
        if (m_timeRemaining < 0) {
            m_timeRemaining = 0;
        }

        m_runTimeElapsed += 1;
        m_levelTimeElapsed += 1;
    }

    emit timerTick(m_timeRemaining);

    if (advancesSharedClock && m_timeRemaining == 0) {
        endRunWithFailure();
        return;
    }

    emit gameUpdated();
    if (advancesSharedClock) {
        sendAuthoritativeState();
    }
}

void Game::endRunWithFailure()
{
    m_sfxGameOver.stop();
    m_sfxGameOver.play();
    m_state = GameState::GAME_OVER;
    m_timer->stop();

    const int finalScore = m_score;
    if (finalScore > m_highScore) {
        m_highScore = finalScore;
    }

    if (m_multiplayerMode) {
        // Brief PLAYER_DEAD state so the joiner sees the transition
        setSessionState(SessionState::PLAYER_DEAD);

        if (m_localPlayerIndex == 0) {
            // Host: move to WAITING_FOR_RESTART_CONFIRMATION and arm timeout.
            // Both flags cleared — we need ACKs from both sides before reloading.
            m_restartReadyFlags[0] = false;
            m_restartReadyFlags[1] = false;
            setSessionState(SessionState::WAITING_FOR_RESTART_CONFIRMATION);
            startSessionTimeout(30000); // 30 s watchdog
        }
        // Joiner: waits for the host's WAITING_FOR_RESTART_CONFIRMATION broadcast,
        // then shows the game-over screen. It sends RestartReady when user presses Restart.
    }

    sendAuthoritativeState();
    emit gameUpdated();
    emit gameOver(false, finalScore);
}

void Game::resetTreasureReachState()
{
    m_playerReachedTreasure = {false, false};
}

bool Game::isCellOccupiedByOtherPlayer(int playerIndex, int x, int y) const
{
    if (!m_multiplayerMode) {
        return false;
    }

    for (int i = 0; i < static_cast<int>(m_players.size()); ++i) {
        if (i == playerIndex || !m_players[i]) {
            continue;
        }
        if (m_players[i]->getX() == x && m_players[i]->getY() == y) {
            const TreasureRoom* treasure = m_currentLevel ? m_currentLevel->getTreasureRoom() : nullptr;
            if (treasure && treasure->getX() == x && treasure->getY() == y) {
                return false;
            }
            return true;
        }
    }
    return false;
}

void Game::handlePlayerReachedTreasure(int playerIndex)
{
    if (!m_multiplayerMode) {
        m_sfxWin.stop();
        m_sfxWin.play();

        if (m_score > m_highScore) {
            m_highScore = m_score;
        }

        if (m_difficulty != Difficulty::EASY) {
            emit clueRevealed("Depth cleared. Descending...");
            nextLevel();
            return;
        }

        m_state = GameState::WIN;
        m_timer->stop();
        emit gameUpdated();
        emit gameOver(true, m_score);
        return;
    }

    m_playerReachedTreasure[playerIndex] = true;
    const bool allReady = m_playerReachedTreasure[0] && m_playerReachedTreasure[1];
    if (!allReady) {
        sendAuthoritativeState();
        emit waitingForTeammate(playerIndex);
        emit clueRevealed("You reached the treasure. Wait for your teammate.");
        emit gameUpdated();
        return;
    }

    if (m_localPlayerIndex != 0) {
        sendAuthoritativeState();
        emit clueRevealed("Both players reached the treasure. Waiting for host to sync the next level.");
        emit gameUpdated();
        return;
    }

    m_sfxWin.stop();
    m_sfxWin.play();
    if (m_score > m_highScore) {
        m_highScore = m_score;
    }
    emit clueRevealed("Both hunters reached the treasure. Descending...");
    nextLevel();
}

// ─────────────────────────────────────────────────────────────────────────────
// Session state machine helpers
// ─────────────────────────────────────────────────────────────────────────────

void Game::setSessionState(SessionState next)
{
    if (m_sessionState == next) return;
    m_sessionState = next;
    // Emit as int — the signal signature uses int to avoid moc type-resolution
    // issues with externally-defined enums. Receivers cast back to SessionState.
    emit sessionStateChanged(static_cast<int>(next));

    // Host broadcasts every state change so the joiner stays in sync.
    if (isAuthoritativeMultiplayerPeer()) {
        using namespace scavenger::net;
        sendGameMessage(makeSessionStateMsg(++m_outSeq, next).toJson());
    }
}

void Game::onRestartReadyReceived(int playerIndex)
{
    if (m_sessionState != SessionState::WAITING_FOR_RESTART_CONFIRMATION) return;
    if (playerIndex < 0 || playerIndex > 1) return;

    m_restartReadyFlags[playerIndex] = true;
    tryCommitRestart();
}

void Game::tryCommitRestart()
{
    // Only commit when BOTH sides have ACKed
    if (!m_restartReadyFlags[0] || !m_restartReadyFlags[1]) return;

    stopSessionTimeout();
    setSessionState(SessionState::RESTARTING);

    // Generate a fresh seed so both sides produce identical procedural maps.
    // This is the single place where m_runIndex and m_runSeedBase advance on restart.
    ++m_runIndex;
    m_runSeedBase = QRandomGenerator::global()->generate();

    const DifficultyProfile profile =
        m_difficultyConfig.selectProfile(m_difficulty, m_runSeedBase, m_runIndex);
    m_activeDifficultyProfileId = profile.id;
    m_activeGenerationRules     = profile.rules;
    m_timeRemaining = qMax(10, m_activeGenerationRules.startingTime);
    m_score = 0;
    m_sharedCoinsCollected = 0;
    m_lootRoomsSpawned = 0;
    m_runTimeElapsed = 0;
    m_levelTimeElapsed = 0;

    // Broadcast the new seed+runIndex BEFORE loading so the joiner can
    // start loading the same map deterministically.  SessionState in the
    // payload is RESTARTING, which authorises the joiner to call startLevel.
    sendAuthoritativeState();

    // Host loads its own copy; startLevel() will send the second StateSync
    // (SessionState still RESTARTING/LOADING) so the joiner also loads.
    startLevel(m_currentLevelIndex, m_difficulty);
    // SessionState transitions to PLAYING when the host receives the joiner's ReadyAck.
}

void Game::startSessionTimeout(int ms)
{
    if (!m_sessionTimeoutTimer) {
        m_sessionTimeoutTimer = new QTimer(this);
        m_sessionTimeoutTimer->setSingleShot(true);
        connect(m_sessionTimeoutTimer, &QTimer::timeout, this, [this]() {
            if (m_sessionState == SessionState::WAITING_FOR_RESTART_CONFIRMATION) {
                setSessionState(SessionState::SESSION_CLOSED);
                emit peerDisconnected(
                    QStringLiteral("Restart acknowledgement timed out. Session closed."));
                stopNetworkThread();
            }
        });
    }
    m_sessionTimeoutTimer->start(ms);
}

void Game::stopSessionTimeout()
{
    if (m_sessionTimeoutTimer)
        m_sessionTimeoutTimer->stop();
}
