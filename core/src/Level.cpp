#include "Level.h"

#include "InteractionResult.h"

#include "ClueManager.h"
#include "ClueTrigger.h"
#include "Coin.h"
#include "HiddenWall.h"
#include "Player.h"
#include "TreasureRoom.h"
#include "TriggerWall.h"
#include "Wall.h"

#include <QtAlgorithms>

quint64 Level::posKey(int x, int y)
{
    // Pack (x,y) into a stable 64-bit key (works with negative too).
    const quint32 ux = static_cast<quint32>(x);
    const quint32 uy = static_cast<quint32>(y);
    return (static_cast<quint64>(ux) << 32) | static_cast<quint64>(uy);
}

Level::Level()
    : m_width(0),
      m_height(0),
      m_timeLimit(0),
      m_spawnX(0),
      m_spawnY(0),
      m_treasureRoom(nullptr)
{
}

Level::~Level()
{
    qDeleteAll(m_objects);
}

void Level::setName(const QString& name)
{
    m_name = name;
}

void Level::setSize(int width, int height)
{
    m_width = width;
    m_height = height;
}

void Level::setTimeLimit(int seconds)
{
    m_timeLimit = seconds;
}

void Level::setSpawn(int x, int y)
{
    m_spawnX = x;
    m_spawnY = y;
}

QString Level::getName() const
{
    return m_name;
}

int Level::getWidth() const
{
    return m_width;
}

int Level::getHeight() const
{
    return m_height;
}

int Level::getTimeLimit() const
{
    return m_timeLimit;
}

QPoint Level::getSpawn() const
{
    return QPoint(m_spawnX, m_spawnY);
}

void Level::setTreasureRoom(TreasureRoom* room)
{
    m_treasureRoom = room;
    addOwnedObject(room);
}

TreasureRoom* Level::getTreasureRoom() const
{
    return m_treasureRoom;
}

void Level::addWall(Wall* wall)
{
    addOwnedObject(wall);
    m_walls.push_back(wall);
    m_wallByPos.insert(posKey(wall->getX(), wall->getY()), wall);
}

void Level::addTriggerWall(TriggerWall* wall)
{
    addOwnedObject(wall);
    m_triggerWalls.push_back(wall);
    m_triggerWallByPos.insert(posKey(wall->getX(), wall->getY()), wall);
}

void Level::addHiddenWall(HiddenWall* wall)
{
    addOwnedObject(wall);
    m_hiddenWalls.push_back(wall);
    m_hiddenWallByPos.insert(posKey(wall->getX(), wall->getY()), wall);
}

void Level::addCoin(Coin* coin)
{
    addOwnedObject(coin);
    m_coins.push_back(coin);
    m_coinByPos.insert(posKey(coin->getX(), coin->getY()), coin);
}

void Level::addClueTrigger(ClueTrigger* clue)
{
    addOwnedObject(clue);
    m_clueTriggers.push_back(clue);
    m_clueTriggerByPos.insert(posKey(clue->getX(), clue->getY()), clue);
}

const QList<GameObject*>& Level::getObjects() const
{
    return m_objects;
}

const QVector<Coin*>& Level::getCoins() const
{
    return m_coins;
}

bool Level::isInBounds(int x, int y) const
{
    return x >= 0 && y >= 0 && x < m_width && y < m_height;
}

bool Level::isWalkable(int x, int y) const
{
    if (!isInBounds(x, y)) {
        return false;
    }

    if (wallAt(x, y) != nullptr) {
        return false;
    }

    if (const TriggerWall* trigger = triggerWallAt(x, y)) {
        if (trigger->isBlocking()) {
            return false;
        }
    }

    if (const HiddenWall* hidden = hiddenWallAt(x, y)) {
        if (hidden->isBlocking()) {
            return false;
        }
    }

    if (m_treasureRoom && m_treasureRoom->getX() == x && m_treasureRoom->getY() == y) {
        return m_treasureRoom->isUnlocked();
    }

    return true;
}

bool Level::canPlayerEnter(int x, int y, const Player& player, QString* outReason) const
{
    if (!isInBounds(x, y)) {
        if (outReason) {
            *outReason = "Out of bounds";
        }
        return false;
    }

    if (wallAt(x, y) != nullptr) {
        if (outReason) {
            *outReason = "Blocked by wall";
        }
        return false;
    }

    if (const TriggerWall* trigger = triggerWallAt(x, y)) {
        if (trigger->isBlocking()) {
            if (outReason) {
                *outReason = "Blocked";
            }
            return false;
        }
    }

    if (const HiddenWall* hidden = hiddenWallAt(x, y)) {
        if (hidden->isBlocking()) {
            if (outReason) {
                *outReason = "Blocked";
            }
            return false;
        }
    }

    if (m_treasureRoom && m_treasureRoom->getX() == x && m_treasureRoom->getY() == y) {
        if (!m_treasureRoom->isUnlocked() && !player.canEnterTreasureRoom()) {
            if (outReason) {
                *outReason = "The treasure room is locked. Collect at least 3 coins!";
            }
            return false;
        }
    }

    return true;
}

Wall* Level::wallAt(int x, int y) const
{
    return m_wallByPos.value(posKey(x, y), nullptr);
}

TriggerWall* Level::triggerWallAt(int x, int y) const
{
    return m_triggerWallByPos.value(posKey(x, y), nullptr);
}

HiddenWall* Level::hiddenWallAt(int x, int y) const
{
    return m_hiddenWallByPos.value(posKey(x, y), nullptr);
}

Coin* Level::coinAt(int x, int y) const
{
    return m_coinByPos.value(posKey(x, y), nullptr);
}

ClueTrigger* Level::clueTriggerAt(int x, int y) const
{
    return m_clueTriggerByPos.value(posKey(x, y), nullptr);
}

InteractionResult Level::interactAt(int x, int y, Player& player, ClueManager& clues)
{
    InteractionResult result;

    if (Coin* coin = coinAt(x, y)) {
        if (!coin->isCollected()) {
            coin->collect();
            player.collectCoin();
            result.coinCollected = true;
            result.coinsCollectedTotal = player.getCoinsCollected();
            result.scoreDelta += coin->getValue();

            result.revealedClues.append(clues.checkCoinClues(player.getCoinsCollected()));
        }
    }

    if (TreasureRoom* treasure = getTreasureRoom()) {
        if (!treasure->isUnlocked() && player.canEnterTreasureRoom()) {
            treasure->unlock();
            result.treasureUnlocked = true;
        }
    }

    if (ClueTrigger* clueTrigger = clueTriggerAt(x, y)) {
        if (!clueTrigger->isActivated()) {
            clueTrigger->activate();
            const QStringList revealed = clues.checkPositionClue(x, y);
            if (revealed.isEmpty()) {
                if (!clueTrigger->getClueText().isEmpty()) {
                    result.revealedClues.push_back(clueTrigger->getClueText());
                }
            } else {
                result.revealedClues.append(revealed);
            }
        }
    }

    if (TriggerWall* triggerWall = triggerWallAt(x, y)) {
        if (!triggerWall->isTriggered()) {
            triggerWall->trigger();
            bool anyOpened = false;
            for (const QPoint& pos : triggerWall->getOpensPositions()) {
                if (HiddenWall* hidden = hiddenWallAt(pos.x(), pos.y())) {
                    if (hidden->isBlocking()) {
                        hidden->open();
                        anyOpened = true;
                    }
                }
            }
            result.wallOpened = anyOpened;
        }
    }

    if (TreasureRoom* treasure = getTreasureRoom()) {
        if (treasure->isUnlocked() && treasure->getX() == x && treasure->getY() == y) {
            result.won = true;
        }
    }

    return result;
}

void Level::addOwnedObject(GameObject* object)
{
    if (!object) {
        return;
    }
    m_objects.push_back(object);
}
