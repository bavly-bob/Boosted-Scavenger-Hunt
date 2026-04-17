#pragma once

#include <QString>

class QPainter;

// Abstract base class for any object positioned on the level grid.
class GameObject {
protected:
    int m_x;
    int m_y;

public:
    GameObject(int x, int y);
    virtual ~GameObject() = default;

    virtual QString getType() const = 0;
    virtual void draw(QPainter& painter, int cellSize) const = 0;

    int getX() const;
    int getY() const;
    void setPosition(int x, int y);
};
