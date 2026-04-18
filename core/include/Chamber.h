#pragma once

#include <QPoint>
#include <QVector>

// Plain data structure describing one room/chamber in the level.
// No Qt parent, no virtual functions – safe to copy/store in QVector.
struct Chamber {
    int id     = 0;
    int x      = 0;   // top-left tile column
    int y      = 0;   // top-left tile row
    int width  = 0;   // width  in tiles (interior only, walls added around it)
    int height = 0;   // height in tiles (interior only)

    // Absolute tile positions of coins placed inside this chamber.
    QVector<QPoint> coinPositions;

    bool hasEnemy = false;
    QPoint enemyPos;

    bool hasTreasureRoom = false;
    QPoint treasurePos;

    // Centre of the chamber in tile coordinates.
    QPoint centre() const
    {
        return QPoint(x + width / 2, y + height / 2);
    }

    bool isValid() const
    {
        return width > 0 && height > 0;
    }
    
};
