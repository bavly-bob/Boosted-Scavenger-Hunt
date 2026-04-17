#pragma once

#include "GameObject.h"

class QPainter;

class Wall : public GameObject {
public:
    Wall(int x, int y);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    virtual bool isBlocking() const;
};

