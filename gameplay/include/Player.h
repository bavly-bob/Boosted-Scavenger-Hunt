#pragma once

#include "Enums.h"
#include "GameObject.h"

class Level;
class QPainter;

class Player : public GameObject {
    int m_coinsCollected;

public:
    Player(int x, int y);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    void move(Direction dir, const Level& level);
    void collectCoin();
    int getCoinsCollected() const;
    bool canEnterTreasureRoom() const;
};

