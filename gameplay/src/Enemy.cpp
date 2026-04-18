#include "Enemy.h"
#include "Level.h"
#include "Player.h"
#include "SpriteManager.h"
#include <QPainter>
#include <cmath>

Enemy::Enemy(int x, int y)
    : GameObject(x, y),
      m_animState(AnimationState::Idle),
      m_animFrame(0),
      m_animTick(0),
      m_moveTimer(0),
      m_moveInterval(2),
      m_sprites(nullptr)
{
}

QString Enemy::getType() const
{
    return "Enemy";
}

void Enemy::draw(QPainter& painter, int cellSize) const
{
    if (m_sprites && m_sprites->drawEnemy(*this, painter, cellSize)) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);

    const int pixelX = m_x * cellSize;
    const int pixelY = m_y * cellSize;
    const int size = cellSize;
    const int margin = size / 8;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(60, 30, 80));
    painter.drawEllipse(pixelX + margin, pixelY + margin, size - 2 * margin, size - 2 * margin);

    const int eyeRadius = qMax(2, size / 10);
    const int eyeY = pixelY + size * 3 / 8;
    painter.setBrush(QColor(255, 30, 30, 80));
    painter.drawEllipse(pixelX + size / 4 - eyeRadius - 1, eyeY - eyeRadius - 1, eyeRadius * 2 + 2, eyeRadius * 2 + 2);
    painter.drawEllipse(pixelX + 3 * size / 4 - eyeRadius - 1, eyeY - eyeRadius - 1, eyeRadius * 2 + 2, eyeRadius * 2 + 2);

    painter.setBrush(QColor(255, 60, 60));
    painter.drawEllipse(pixelX + size / 4 - eyeRadius, eyeY - eyeRadius, eyeRadius * 2, eyeRadius * 2);
    painter.drawEllipse(pixelX + 3 * size / 4 - eyeRadius, eyeY - eyeRadius, eyeRadius * 2, eyeRadius * 2);

    painter.setPen(QPen(QColor(120, 60, 160), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(pixelX + margin, pixelY + margin, size - 2 * margin, size - 2 * margin);

    if (isDead()) {
        painter.setPen(QPen(QColor(255, 255, 255, 120), 2));
        painter.drawLine(pixelX + margin, pixelY + margin, pixelX + size - margin, pixelY + size - margin);
        painter.drawLine(pixelX + size - margin, pixelY + margin, pixelX + margin, pixelY + size - margin);
    }

    painter.restore();
}

void Enemy::update(Level& level, const Player& player)
{
    if (isDead()) return;

    m_moveTimer++;
    if (m_moveTimer < m_moveInterval) return;
    m_moveTimer = 0;

    int dx = player.getX() - m_x;
    int dy = player.getY() - m_y;

    if (dx == 0 && dy == 0) return;

    int nextX = m_x;
    int nextY = m_y;
    Direction dir = Direction::None;

    if (std::abs(dx) > std::abs(dy)) {
        nextX += (dx > 0) ? 1 : -1;
        dir = (dx > 0) ? Direction::Right : Direction::Left;
        if (!level.isWalkable(nextX, nextY)) {
            nextX = m_x;
            nextY += (dy > 0) ? 1 : (dy < 0 ? -1 : 0);
            dir = (dy > 0) ? Direction::Down : (dy < 0 ? Direction::Up : Direction::None);
        }
    } else {
        nextY += (dy > 0) ? 1 : -1;
        dir = (dy > 0) ? Direction::Down : Direction::Up;
        if (!level.isWalkable(nextX, nextY)) {
            nextY = m_y;
            nextX += (dx > 0) ? 1 : (dx < 0 ? -1 : 0);
            dir = (dx > 0) ? Direction::Right : (dx < 0 ? Direction::Left : Direction::None);
        }
    }

    if (level.isWalkable(nextX, nextY)) {
        m_x = nextX;
        m_y = nextY;

        switch (dir) {
        case Direction::Up:    m_animState = AnimationState::MovingUp; break;
        case Direction::Down:  m_animState = AnimationState::MovingDown; break;
        case Direction::Left:  m_animState = AnimationState::MovingLeft; break;
        case Direction::Right: m_animState = AnimationState::MovingRight; break;
        default:               m_animState = AnimationState::Idle; break;
        }
    } else {
        m_animState = AnimationState::Idle;
    }
}

void Enemy::advanceAnimation()
{
    if (!m_sprites) return;

    QString clipName = "enemy_idle";
    switch (m_animState) {
    case AnimationState::MovingUp:    clipName = "enemy_move_up";    break;
    case AnimationState::MovingDown:  clipName = "enemy_move_down";  break;
    case AnimationState::MovingLeft:  clipName = "enemy_move_left";  break;
    case AnimationState::MovingRight: clipName = "enemy_move_right"; break;
    case AnimationState::Dying:       clipName = "enemy_die";        break;
    default:                          clipName = "enemy_idle";       break;
    }

    const AnimationClip* clip = m_sprites->clip(clipName);
    if (clip) {
        if (clip->fps > 0) {
            const int ticksPerFrame = qMax(1, 60 / clip->fps);
            if (++m_animTick >= ticksPerFrame) {
                m_animTick = 0;
                m_animFrame++;
                if (m_animState == AnimationState::Dying && m_animFrame >= clip->frameCount) {
                    m_animFrame = clip->frameCount - 1;
                } else {
                    m_animFrame %= clip->frameCount;
                }
            }
        }
    }
}

void Enemy::setSpriteManager(const SpriteManager* sm)
{
    m_sprites = sm;
}

void Enemy::setDying()
{
    m_animState = AnimationState::Dying;
    m_animFrame = 0;
    m_animTick = 0;
}

bool Enemy::isDead() const
{
    return m_animState == AnimationState::Dying;
}
