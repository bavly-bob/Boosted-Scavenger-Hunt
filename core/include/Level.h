#pragma once

#include <QPoint>
#include <QString>
#include <QVector>

class ClueTrigger;
class Coin;
class GameObject;
class HiddenWall;
class TreasureRoom;
class TriggerWall;
class Wall;

class Level {
    QString m_name;
    int m_width;
    int m_height;
    int m_timeLimit;
    int m_spawnX;
    int m_spawnY;

    QVector<GameObject*> m_objects; // owning

    QVector<Wall*> m_walls;
    QVector<TriggerWall*> m_triggerWalls;
    QVector<HiddenWall*> m_hiddenWalls;
    QVector<Coin*> m_coins;
    QVector<ClueTrigger*> m_clueTriggers;
    TreasureRoom* m_treasureRoom;

public:
    Level();
    ~Level();

    void setName(const QString& name);
    void setSize(int width, int height);
    void setTimeLimit(int seconds);
    void setSpawn(int x, int y);

    QString getName() const;
    int getWidth() const;
    int getHeight() const;
    int getTimeLimit() const;
    QPoint getSpawn() const;

    void setTreasureRoom(TreasureRoom* room);
    TreasureRoom* getTreasureRoom() const;

    void addWall(Wall* wall);
    void addTriggerWall(TriggerWall* wall);
    void addHiddenWall(HiddenWall* wall);
    void addCoin(Coin* coin);
    void addClueTrigger(ClueTrigger* clue);

    const QVector<GameObject*>& getObjects() const;
    const QVector<Coin*>& getCoins() const;

    bool isInBounds(int x, int y) const;
    bool isWalkable(int x, int y) const;

    Wall* wallAt(int x, int y) const;
    TriggerWall* triggerWallAt(int x, int y) const;
    HiddenWall* hiddenWallAt(int x, int y) const;
    Coin* coinAt(int x, int y) const;
    ClueTrigger* clueTriggerAt(int x, int y) const;

private:
    void addOwnedObject(GameObject* object);
};

