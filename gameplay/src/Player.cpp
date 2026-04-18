#include "Player.h"

#include "Level.h"
#include "PlayerAnimator.h"
#include "SpriteManager.h"

#include <QPainter>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
Player::Player(int x, int y)
    : GameObject(x, y),
      m_coinsCollected(0),
      m_targetX(x),
      m_targetY(y),
      m_pixelX(static_cast<float>(x)),
      m_pixelY(static_cast<float>(y)),
      m_prevPixelX(static_cast<float>(x)),
      m_prevPixelY(static_cast<float>(y)),
      m_isMoving(false),
      m_moveProgress(1.0f),
      m_animator(),
      m_sprites(nullptr)
{
}

QString Player::getType() const
{
    return "Player";
}

// ─────────────────────────────────────────────────────────────────────────────
// draw — uses sub-tile pixel position for smooth rendering
// ─────────────────────────────────────────────────────────────────────────────
void Player::draw(QPainter& painter, int cellSize) const
{
    const int px = static_cast<int>(std::round(m_pixelX * cellSize));
    const int py = static_cast<int>(std::round(m_pixelY * cellSize));

    // Attempt sprite-based rendering
    if (m_sprites) {
        // When idle, draw the last-direction clip at frame 0.
        const AnimationState drawSt = (m_animator.animState() == AnimationState::Idle)
                                      ? m_animator.lastDir()
                                      : m_animator.animState();
        const QString clip = PlayerAnimator::clipName(drawSt);

        const AnimationClip* c = m_sprites->clip(clip);
        if (c) {
            const QPixmap& sheet = m_sprites->sprite(c->spriteKey);
            if (!sheet.isNull()) {
                const int frame = (m_animator.animState() == AnimationState::Idle)
                                  ? 0
                                  : (m_animator.animFrame() % c->frameCount);
                const QRect srcRect(frame * c->frameWidth, c->srcY, c->frameWidth, c->frameHeight);
                const QRect dstRect(px, py, cellSize, cellSize);
                painter.drawPixmap(dstRect, sheet, srcRect);
                return;
            }
        }
    }

    // ── Procedural fallback ──────────────────────────────────────────────────
    const QRect cellRect(px, py, cellSize, cellSize);
    const int margin = cellSize / 6;
    const QRect body = cellRect.adjusted(margin, margin, -margin, -margin);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(80, 160, 255, 60));
    painter.drawEllipse(body.adjusted(-3, -3, 3, 3));

    QColor bodyColor;
    switch (m_animator.animState()) {
    case AnimationState::MovingUp:    bodyColor = QColor(100, 200, 255); break;
    case AnimationState::MovingDown:  bodyColor = QColor(80,  170, 240); break;
    case AnimationState::MovingLeft:  bodyColor = QColor(70,  150, 230); break;
    case AnimationState::MovingRight: bodyColor = QColor(90,  180, 250); break;
    default:                          bodyColor = QColor(70,  120, 220); break;
    }
    painter.setBrush(bodyColor);
    painter.setPen(QPen(QColor(25, 30, 55), 2));
    painter.drawEllipse(body);

    const QPoint centre = body.center();
    const int dotR = cellSize / 8;
    QPoint dotOffset;
    switch (m_animator.lastDir()) {
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

// ─────────────────────────────────────────────────────────────────────────────
// updateMovement — called each render frame with delta time in seconds.
//
// FIX (lerp pop): we now interpolate from m_prevPixelX/Y (the pixel position
// recorded at the moment move() was called) rather than from m_x/m_y (which
// is already snapped to the target tile the instant move() fires).  This
// eliminates the 1-frame jump when a new step begins mid-animation.
// ─────────────────────────────────────────────────────────────────────────────
void Player::updateMovement(float dt)
{
    if (!m_isMoving) return;

    m_moveProgress += MOVE_SPEED * dt;
    if (m_moveProgress >= 1.0f) {
        m_moveProgress = 1.0f;
        m_pixelX = static_cast<float>(m_targetX);
        m_pixelY = static_cast<float>(m_targetY);
        m_isMoving = false;
        m_animator.notifyArrived();
    } else {
        // Lerp from the previous pixel pos (not the snapped logical pos)
        m_pixelX = m_prevPixelX + (static_cast<float>(m_targetX) - m_prevPixelX) * m_moveProgress;
        m_pixelY = m_prevPixelY + (static_cast<float>(m_targetY) - m_prevPixelY) * m_moveProgress;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// move — initiates movement to an adjacent tile
// ─────────────────────────────────────────────────────────────────────────────
void Player::move(Direction dir, const Level& level)
{
    int newX = m_targetX;
    int newY = m_targetY;

    AnimationState movingDir = AnimationState::Idle;

    switch (dir) {
    case Direction::None:  return;
    case Direction::Up:    newY -= 1; movingDir = AnimationState::MovingUp;    break;
    case Direction::Down:  newY += 1; movingDir = AnimationState::MovingDown;  break;
    case Direction::Left:  newX -= 1; movingDir = AnimationState::MovingLeft;  break;
    case Direction::Right: newX += 1; movingDir = AnimationState::MovingRight; break;
    }

    if (!level.isWalkable(newX, newY)) {
        return; // don't change animator state on a blocked move
    }

    // Snapshot current pixel position before updating logical position.
    // This is the lerp origin; using m_x/m_y after setPosition() would pop.
    m_prevPixelX = m_pixelX;
    m_prevPixelY = m_pixelY;

    // Update logical position immediately (collision / interaction logic reads this)
    setPosition(newX, newY);

    m_targetX      = newX;
    m_targetY      = newY;
    m_isMoving     = true;
    m_moveProgress = 0.0f;

    m_animator.notifyMoveStarted(movingDir);
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

bool Player::isAtTarget() const
{
    return !m_isMoving;
}

// ── Animation delegation ─────────────────────────────────────────────────────

void Player::advanceAnimation()
{
    m_animator.tick(m_sprites);
}

AnimationState Player::animState() const
{
    return m_animator.animState();
}

int Player::animFrame() const
{
    return m_animator.animFrame();
}

void Player::setSpriteManager(const SpriteManager* sm)
{
    m_sprites = sm;
}
