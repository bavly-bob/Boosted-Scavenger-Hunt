#include "Player.h"

#include "Level.h"
#include "SpriteManager.h"

#include <QPainter>

// Ticks (game-loop iterations) per animation frame when no clip is registered.
static constexpr int DEFAULT_ANIM_TICKS = 2;

Player::Player(int x, int y)
    : GameObject(x, y),
      m_coinsCollected(0),
      m_animState(AnimationState::Idle),
      m_animFrame(0),
      m_animTick(DEFAULT_ANIM_TICKS),
      m_sprites(nullptr)
{
}

QString Player::getType() const
{
    return "Player";
}

void Player::draw(QPainter& painter, int cellSize) const
{
    // Attempt sprite-based rendering via SpriteManager.
    if (m_sprites && m_sprites->drawPlayer(*this, painter, cellSize)) {
        return;
    }

    // ── Procedural fallback ──────────────────────────────────────────────────
    const QRect cellRect(m_x * cellSize, m_y * cellSize, cellSize, cellSize);
    const int margin = cellSize / 6;
    const QRect body = cellRect.adjusted(margin, margin, -margin, -margin);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Glow ring
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(80, 160, 255, 60));
    painter.drawEllipse(body.adjusted(-3, -3, 3, 3));

    // Body
    QColor bodyColor;
    switch (m_animState) {
    case AnimationState::MovingUp:    bodyColor = QColor(100, 200, 255); break;
    case AnimationState::MovingDown:  bodyColor = QColor(80,  170, 240); break;
    case AnimationState::MovingLeft:  bodyColor = QColor(70,  150, 230); break;
    case AnimationState::MovingRight: bodyColor = QColor(90,  180, 250); break;
    default:                          bodyColor = QColor(70,  120, 220); break;
    }
    painter.setBrush(bodyColor);
    painter.setPen(QPen(QColor(25, 30, 55), 2));
    painter.drawEllipse(body);

    // Direction indicator dot
    const QPoint centre = body.center();
    const int dotR = cellSize / 8;
    QPoint dotOffset;
    switch (m_animState) {
    case AnimationState::MovingUp:    dotOffset = QPoint(0, -cellSize / 6); break;
    case AnimationState::MovingDown:  dotOffset = QPoint(0,  cellSize / 6); break;
    case AnimationState::MovingLeft:  dotOffset = QPoint(-cellSize / 6, 0); break;
    case AnimationState::MovingRight: dotOffset = QPoint( cellSize / 6, 0); break;
    default:                          dotOffset = QPoint(0, 0);             break;
    }
    painter.setBrush(QColor(230, 240, 255, 200));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(centre + dotOffset, dotR, dotR);

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
        m_animState = AnimationState::MovingUp;
        break;
    case Direction::Down:
        newY += 1;
        m_animState = AnimationState::MovingDown;
        break;
    case Direction::Left:
        newX -= 1;
        m_animState = AnimationState::MovingLeft;
        break;
    case Direction::Right:
        newX += 1;
        m_animState = AnimationState::MovingRight;
        break;
    }

    if (!level.isWalkable(newX, newY)) {
        m_animState = AnimationState::Idle;
        return;
    }

    setPosition(newX, newY);
    m_animFrame = 0;
    m_animTick  = DEFAULT_ANIM_TICKS;
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

AnimationState Player::animState() const
{
    return m_animState;
}

int Player::animFrame() const
{
    return m_animFrame;
}

void Player::advanceAnimation()
{
    if (m_animState == AnimationState::Idle) {
        m_animFrame = 0;
        return;
    }

    // Determine clip length (or use default).
    int clipLen = 4; // default frames for moving
    if (m_sprites) {
        const QString clipName = [this]() -> QString {
            switch (m_animState) {
            case AnimationState::MovingUp:    return "player_move_up";
            case AnimationState::MovingDown:  return "player_move_down";
            case AnimationState::MovingLeft:  return "player_move_left";
            case AnimationState::MovingRight: return "player_move_right";
            default:                          return "player_idle";
            }
        }();
        if (const AnimationClip* c = m_sprites->clip(clipName)) {
            clipLen = c->frameCount;
        }
    }

    if (--m_animTick <= 0) {
        m_animTick = DEFAULT_ANIM_TICKS;
        m_animFrame = (m_animFrame + 1) % clipLen;
        // After one full cycle, return to idle.
        if (m_animFrame == 0) {
            m_animState = AnimationState::Idle;
        }
    }
}

void Player::setSpriteManager(const SpriteManager* sm)
{
    m_sprites = sm;
}
