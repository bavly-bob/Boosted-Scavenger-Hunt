#include "TreasureRoom.h"

#include <QPainter>

TreasureRoom::TreasureRoom(int x, int y)
    : GameObject(x, y),
      m_unlocked(false)
{
}

QString TreasureRoom::getType() const
{
    return "TreasureRoom";
}

void TreasureRoom::draw(QPainter& painter, int cellSize) const
{
    const QRect cellRect(m_x * cellSize, m_y * cellSize, cellSize, cellSize);
    const QRect chestRect = cellRect.adjusted(cellSize / 6, cellSize / 4, -cellSize / 6, -cellSize / 6);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(QPen(QColor(60, 40, 20), 2));
    painter.setBrush(m_unlocked ? QColor(220, 170, 60) : QColor(120, 120, 130));
    painter.drawRoundedRect(chestRect, 6, 6);

    painter.setPen(QPen(m_unlocked ? QColor(255, 240, 190) : QColor(220, 80, 70), 2));
    painter.drawLine(chestRect.left(), chestRect.center().y(), chestRect.right(), chestRect.center().y());

    painter.restore();
}

void TreasureRoom::unlock()
{
    m_unlocked = true;
}

bool TreasureRoom::isUnlocked() const
{
    return m_unlocked;
}

