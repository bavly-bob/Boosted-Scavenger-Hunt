#pragma once

#include "GameObject.h"

class QPainter;

class TreasureRoom : public GameObject {
    bool m_unlocked;

public:
    TreasureRoom(int x, int y);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    void unlock();
    bool isUnlocked() const;
};

