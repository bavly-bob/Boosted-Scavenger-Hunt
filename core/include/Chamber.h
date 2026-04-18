#pragma once

#include <QPoint>
#include <QVector>

struct Chamber {
    int id     = 0;
    int x      = 0;
    int y      = 0;
    int width  = 0;
    int height = 0;

    QVector<QPoint> coinPositions;

    bool hasEnemy = false;
    QPoint enemyPos;

    bool hasTreasureRoom = false;
    QPoint treasurePos;

    QPoint centre() const
    {
        return QPoint(x + width / 2, y + height / 2);
    }

    bool isValid() const
    {
        return width > 0 && height > 0;
    }
};
