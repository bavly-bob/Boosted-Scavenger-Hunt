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
      m_moveInterval(2), // Move every 2 ticks
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

    // ── Pixel-art skull fallback ─────────────────────────────────────────────
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false); // crisp pixels

    const int x = m_x * cellSize;
    const int y = m_y * cellSize;
    const int s = cellSize;
    const int m = s / 8; // margin

    // Body (dark purple/grey base)
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(60, 30, 80));
    painter.drawEllipse(x + m, y + m, s - 2*m, s - 2*m);

    // Glowing red eyes
    const int eyeR = qMax(2, s / 10);
    const int eyeY = y + s * 3 / 8;
    // Left eye glow
    painter.setBrush(QColor(255, 30, 30, 80));
    painter.drawEllipse(x + s/4 - eyeR - 1, eyeY - eyeR - 1, eyeR*2+2, eyeR*2+2);
    // Right eye glow
    painter.drawEllipse(x + 3*s/4 - eyeR - 1, eyeY - eyeR - 1, eyeR*2+2, eyeR*2+2);
    // Left eye
    painter.setBrush(QColor(255, 60, 60));
    painter.drawEllipse(x + s/4 - eyeR, eyeY - eyeR, eyeR*2, eyeR*2);
    // Right eye
    painter.drawEllipse(x + 3*s/4 - eyeR, eyeY - eyeR, eyeR*2, eyeR*2);

    // Rim outline
    painter.setPen(QPen(QColor(120, 60, 160), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(x + m, y + m, s - 2*m, s - 2*m);

    // Death indicator when dying
    if (isDead()) {
        painter.setPen(QPen(QColor(255, 255, 255, 120), 2));
        painter.drawLine(x + m, y + m, x + s - m, y + s - m);
        painter.drawLine(x + s - m, y + m, x + m, y + s - m);
    }

    painter.restore();
}

void Enemy::update(Level& level, const Player& player)
{
    if (isDead()) return;

    m_moveTimer++;
    if (m_moveTimer < m_moveInterval) return;
    m_moveTimer = 0;

    // Simple pathfinding: move towards player if possible
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
            nextX = m_x; // reset
            nextY += (dy > 0) ? 1 : (dy < 0 ? -1 : 0);
            dir = (dy > 0) ? Direction::Down : (dy < 0 ? Direction::Up : Direction::None);
        }
    } else {
        nextY += (dy > 0) ? 1 : -1;
        dir = (dy > 0) ? Direction::Down : Direction::Up;
        if (!level.isWalkable(nextX, nextY)) {
            nextY = m_y; // reset
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
                    m_animFrame = clip->frameCount - 1; // Stop at last frame of dying
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
