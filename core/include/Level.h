#pragma once

#include "Chamber.h"
#include "Enums.h"

#include <QPoint>
#include <QHash>
#include <QtGlobal>
#include <QString>
#include <QList>
#include <QVector>

class ClueManager;
class ClueTrigger;
class Coin;
class GameObject;
class HiddenWall;
struct InteractionResult;
class Player;
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

    // Target architecture: layered grids + objects.
    QVector<QVector<int>> m_tiles;
    QVector<QVector<bool>> m_collision;
    QList<GameObject*> m_objects; // owning

    QVector<Wall*> m_walls;
    QVector<TriggerWall*> m_triggerWalls;
    QVector<HiddenWall*> m_hiddenWalls;
    QVector<Coin*> m_coins;
    QVector<ClueTrigger*> m_clueTriggers;
    TreasureRoom* m_treasureRoom;

    // Fast spatial lookup (tileX,tileY) -> object pointer.
    QHash<quint64, Wall*> m_wallByPos;
    QHash<quint64, TriggerWall*> m_triggerWallByPos;
    QHash<quint64, HiddenWall*> m_hiddenWallByPos;
    QHash<quint64, Coin*> m_coinByPos;
    QHash<quint64, ClueTrigger*> m_clueTriggerByPos;

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

    const QVector<QVector<int>>& tiles() const;
    const QVector<QVector<bool>>& collision() const;

    int tileAt(int x, int y) const;
    bool isCollidingAt(int x, int y) const;
    void setTileAt(int x, int y, int tileId);
    void setCollisionAt(int x, int y, bool colliding);

    void setTreasureRoom(TreasureRoom* room);
    TreasureRoom* getTreasureRoom() const;

    void addWall(Wall* wall);
    void addTriggerWall(TriggerWall* wall);
    void addHiddenWall(HiddenWall* wall);
    void addCoin(Coin* coin);
    void addClueTrigger(ClueTrigger* clue);

    const QList<GameObject*>& getObjects() const;
    const QVector<Coin*>& getCoins() const;

    bool isInBounds(int x, int y) const;
    bool isWalkable(int x, int y) const;

    // Checks whether the player can enter a tile; if not, returns a reason.
    bool canPlayerEnter(int x, int y, const Player& player, QString* outReason = nullptr) const;

    Wall* wallAt(int x, int y) const;
    TriggerWall* triggerWallAt(int x, int y) const;
    HiddenWall* hiddenWallAt(int x, int y) const;
    Coin* coinAt(int x, int y) const;
    ClueTrigger* clueTriggerAt(int x, int y) const;

    // Applies side-effects for stepping onto a tile (coins, triggers, etc.).
    InteractionResult interactAt(int x, int y, Player& player, ClueManager& clues);

    // ── Chamber support ───────────────────────────────────────────────────
    void addChamber(const Chamber& ch);
    const QVector<Chamber>& getChambers() const;
    int chamberCount() const;

private:
    QVector<Chamber> m_chambers;

    static quint64 posKey(int x, int y);
    void setTileAndCollision(int x, int y, int tileId, bool colliding);
    void addOwnedObject(GameObject* object);
};

