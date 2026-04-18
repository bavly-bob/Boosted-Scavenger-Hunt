#include "HiddenWall.h"

#include <QPainter>

HiddenWall::HiddenWall(int x, int y)
    : Wall(x, y),
      m_open(false)
{
}

QString HiddenWall::getType() const
{
    return "HiddenWall";
}

void HiddenWall::draw(QPainter& painter, int cellSize) const
{
    if (m_open) {
        return;
    }
    // Hidden walls should visually blend with normal walls until opened.
    Wall::draw(painter, cellSize);
}

bool HiddenWall::isBlocking() const
{
    return !m_open;
}

void HiddenWall::open()
{
    m_open = true;
}

bool HiddenWall::isOpen() const
{
    return m_open;
}
