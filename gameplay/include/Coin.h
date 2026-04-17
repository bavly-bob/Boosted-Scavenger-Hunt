#pragma once

#include "GameObject.h"

class QPainter;

class Coin : public GameObject {
    int m_value;
    bool m_collected;

public:
    Coin(int x, int y, int value = 100);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    void collect();
    bool isCollected() const;
    int getValue() const;
};

