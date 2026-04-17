#include "Wall.h"

#include <QPainter>

Wall::Wall(int x, int y)
    : GameObject(x, y)
{
}

QString Wall::getType() const
{
    return "Wall";
}

void Wall::draw(QPainter& painter, int cellSize) const
{
    const QRect r(m_x * cellSize, m_y * cellSize, cellSize, cellSize);

    painter.save();
    painter.setPen(QPen(QColor(35, 35, 40), 1));
    painter.setBrush(QColor(65, 65, 75));
    painter.drawRect(r.adjusted(1, 1, -1, -1));
    painter.restore();
}

bool Wall::isBlocking() const
{
    return true;
}

