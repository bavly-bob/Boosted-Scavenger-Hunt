#include "TriggerWall.h"

#include <QPainter>

TriggerWall::TriggerWall(int x, int y, const QVector<QPoint>& opens)
    : Wall(x, y),
      m_opensPositions(opens),
      m_triggered(false)
{
}

QString TriggerWall::getType() const
{
    return "TriggerWall";
}

void TriggerWall::draw(QPainter& painter, int cellSize) const
{
    const QRect r(m_x * cellSize, m_y * cellSize, cellSize, cellSize);

    painter.save();
    painter.setPen(QPen(QColor(45, 45, 50), 1));
    painter.setBrush(QColor(70, 70, 80));
    painter.drawRect(r.adjusted(1, 1, -1, -1));

    painter.setPen(QPen(m_triggered ? QColor(80, 200, 120) : QColor(230, 180, 60), 2));
    painter.drawRect(r.adjusted(4, 4, -4, -4));
    painter.restore();
}

bool TriggerWall::isBlocking() const
{
    return false;
}

void TriggerWall::trigger()
{
    m_triggered = true;
}

bool TriggerWall::isTriggered() const
{
    return m_triggered;
}

QVector<QPoint> TriggerWall::getOpensPositions() const
{
    return m_opensPositions;
}
