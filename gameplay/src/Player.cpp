#include "Player.h"

#include "Level.h"

#include <QPainter>

Player::Player(int x, int y)
    : GameObject(x, y),
      m_coinsCollected(0)
{
}

QString Player::getType() const
{
    return "Player";
}

void Player::draw(QPainter& painter, int cellSize) const
{
    const QRect cellRect(m_x * cellSize, m_y * cellSize, cellSize, cellSize);
    const int margin = cellSize / 6;
    const QRect playerRect = cellRect.adjusted(margin, margin, -margin, -margin);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(25, 30, 55), 2));
    painter.setBrush(QColor(70, 120, 220));
    painter.drawEllipse(playerRect);
    painter.restore();
}

void Player::move(Direction dir, const Level& level)
{
    int newX = m_x;
    int newY = m_y;

    switch (dir) {
    case Direction::None:
        return;
    case Direction::Up:
        newY -= 1;
        break;
    case Direction::Down:
        newY += 1;
        break;
    case Direction::Left:
        newX -= 1;
        break;
    case Direction::Right:
        newX += 1;
        break;
    }

    if (!level.isWalkable(newX, newY)) {
        return;
    }

    setPosition(newX, newY);
}

void Player::collectCoin()
{
    m_coinsCollected += 1;
}

int Player::getCoinsCollected() const
{
    return m_coinsCollected;
}

bool Player::canEnterTreasureRoom() const
{
    return m_coinsCollected >= 3;
}
