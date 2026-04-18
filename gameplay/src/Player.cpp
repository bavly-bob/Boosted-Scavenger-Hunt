#include "Player.h"

#include "Level.h"
#include "PlayerAnimator.h"
#include "SpriteManager.h"

#include <QPainter>
#include <cmath>

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

void Player::draw(QPainter& painter, int cellSize) const
{
    const int px = static_cast<int>(std::round(m_pixelX * cellSize));
    const int py = static_cast<int>(std::round(m_pixelY * cellSize));

    if (m_sprites) {
        const AnimationState drawState = (m_animator.animState() == AnimationState::Idle)
                                         ? m_animator.lastDir()
                                         : m_animator.animState();
        const QString clipKey = PlayerAnimator::clipName(drawState);

        const AnimationClip* clip = m_sprites->clip(clipKey);
        if (clip && clip->frameCount > 0) {
            const int frame = (m_animator.animState() == AnimationState::Idle)
                              ? 0
                              : (m_animator.animFrame() % clip->frameCount);

            // Individual-frame mode stores each frame as "<clipKey>_frame_<N>".
            const QString frameKey = QString("%1_frame_%2").arg(clipKey).arg(frame);
            const QPixmap& framePix = m_sprites->sprite(frameKey);
            if (!framePix.isNull()) {
                painter.drawPixmap(QRect(px, py, cellSize, cellSize), framePix,
                                   framePix.rect());
                return;
            }

            const QPixmap& sheet = m_sprites->sprite(clip->spriteKey);
            if (!sheet.isNull()) {
                const QRect srcRect(frame * clip->frameWidth, clip->srcY,
                                    clip->frameWidth, clip->frameHeight);
                painter.drawPixmap(QRect(px, py, cellSize, cellSize), sheet, srcRect);
                return;
            }
        }
    }

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

// Interpolate from the position captured at move start to avoid a one-frame
// visual snap when logical tile coordinates update immediately.
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
        m_pixelX = m_prevPixelX + (static_cast<float>(m_targetX) - m_prevPixelX) * m_moveProgress;
        m_pixelY = m_prevPixelY + (static_cast<float>(m_targetY) - m_prevPixelY) * m_moveProgress;
    }
}

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
        return;
    }

    // Capture interpolation origin before setPosition updates tile coordinates.
    m_prevPixelX = m_pixelX;
    m_prevPixelY = m_pixelY;

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
