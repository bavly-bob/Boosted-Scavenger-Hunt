#include "Coin.h"

#include <QPainter>

Coin::Coin(int x, int y, int value)
    : GameObject(x, y),
      m_value(value),
      m_collected(false)
{
}

QString Coin::getType() const
{
    return "Coin";
}

void Coin::draw(QPainter& painter, int cellSize) const
{
    if (m_collected) {
        return;
    }

    const QRect cellRect(m_x * cellSize, m_y * cellSize, cellSize, cellSize);
    const int margin = cellSize / 5;
    const QRect coinRect = cellRect.adjusted(margin, margin, -margin, -margin);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(160, 120, 20), 2));
    painter.setBrush(QColor(240, 200, 60));
    painter.drawEllipse(coinRect);
    painter.restore();
}

void Coin::collect()
{
    m_collected = true;
}

bool Coin::isCollected() const
{
    return m_collected;
}

int Coin::getValue() const
{
    return m_value;
}

