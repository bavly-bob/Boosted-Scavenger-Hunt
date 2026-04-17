#pragma once

#include "Wall.h"

class QPainter;

class HiddenWall : public Wall {
    bool m_open;

public:
    HiddenWall(int x, int y);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    bool isBlocking() const override;

    void open();
    bool isOpen() const;
};

