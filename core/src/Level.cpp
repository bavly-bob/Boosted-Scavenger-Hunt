#include "Level.h"

#include "ClueTrigger.h"
#include "Coin.h"
#include "HiddenWall.h"
#include "TreasureRoom.h"
#include "TriggerWall.h"
#include "Wall.h"

#include <QtAlgorithms>

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
}

void Level::addTriggerWall(TriggerWall* wall)
{
    addOwnedObject(wall);
    m_triggerWalls.push_back(wall);
}

void Level::addHiddenWall(HiddenWall* wall)
{
    addOwnedObject(wall);
    m_hiddenWalls.push_back(wall);
}

void Level::addCoin(Coin* coin)
{
    addOwnedObject(coin);
    m_coins.push_back(coin);
}

void Level::addClueTrigger(ClueTrigger* clue)
{
    addOwnedObject(clue);
    m_clueTriggers.push_back(clue);
}

const QVector<GameObject*>& Level::getObjects() const
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

Wall* Level::wallAt(int x, int y) const
{
    for (Wall* wall : m_walls) {
        if (wall->getX() == x && wall->getY() == y) {
            return wall;
        }
    }
    return nullptr;
}

TriggerWall* Level::triggerWallAt(int x, int y) const
{
    for (TriggerWall* wall : m_triggerWalls) {
        if (wall->getX() == x && wall->getY() == y) {
            return wall;
        }
    }
    return nullptr;
}

HiddenWall* Level::hiddenWallAt(int x, int y) const
{
    for (HiddenWall* wall : m_hiddenWalls) {
        if (wall->getX() == x && wall->getY() == y) {
            return wall;
        }
    }
    return nullptr;
}

Coin* Level::coinAt(int x, int y) const
{
    for (Coin* coin : m_coins) {
        if (coin->getX() == x && coin->getY() == y) {
            return coin;
        }
    }
    return nullptr;
}

ClueTrigger* Level::clueTriggerAt(int x, int y) const
{
    for (ClueTrigger* clue : m_clueTriggers) {
        if (clue->getX() == x && clue->getY() == y) {
            return clue;
        }
    }
    return nullptr;
}

void Level::addOwnedObject(GameObject* object)
{
    if (!object) {
        return;
    }
    m_objects.push_back(object);
}
