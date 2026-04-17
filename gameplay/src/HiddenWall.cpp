#include "HiddenWall.h"

#include <QPainter>
#include <QPoint>

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

    const QRect r(m_x * cellSize, m_y * cellSize, cellSize, cellSize);

    painter.save();
    painter.setPen(QPen(QColor(35, 35, 40), 1));
    painter.setBrush(QColor(65, 65, 75));
    painter.drawRect(r.adjusted(1, 1, -1, -1));

    painter.setPen(QPen(QColor(120, 110, 160), 2));
    painter.drawLine(r.topLeft() + QPoint(6, 6), r.bottomRight() - QPoint(6, 6));
    painter.restore();
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
