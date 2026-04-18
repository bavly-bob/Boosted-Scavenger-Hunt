#include "Player.h"

#include "Level.h"
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
      m_isMoving(false),
      m_moveProgress(1.0f),
      m_animState(AnimationState::Idle),
      m_animFrame(0),
      m_animTick(ANIM_TICKS_PER_FRAME),
      m_lastDir(AnimationState::MovingDown),
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
    // Pixel coords in tile-space units (converted to actual px by caller via translate)
    const int px = static_cast<int>(std::round(m_pixelX * cellSize));
    const int py = static_cast<int>(std::round(m_pixelY * cellSize));

    // Attempt sprite-based rendering
    if (m_sprites) {
        // Determine which clip to use
        AnimationState drawState = (m_animState == AnimationState::Idle) ? m_lastDir : m_animState;
        QString clipName;
        switch (drawState) {
        case AnimationState::MovingUp:    clipName = "player_move_up";    break;
        case AnimationState::MovingDown:  clipName = "player_move_down";  break;
        case AnimationState::MovingLeft:  clipName = "player_move_left";  break;
        case AnimationState::MovingRight: clipName = "player_move_right"; break;
        default:                          clipName = "player_move_down";  break;
        }

        const AnimationClip* c = m_sprites->clip(clipName);
        if (c) {
            const QPixmap& sheet = m_sprites->sprite(c->spriteKey);
            if (!sheet.isNull()) {
                // When idle, always show frame 0; when moving, use m_animFrame
                const int frame = (m_animState == AnimationState::Idle) ? 0 : (m_animFrame % c->frameCount);
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
    switch (m_lastDir) {
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
// updateMovement — called each render frame with delta time in seconds
// ─────────────────────────────────────────────────────────────────────────────
void Player::updateMovement(float dt, int tileSize)
{
    Q_UNUSED(tileSize);
    if (!m_isMoving) return;

    m_moveProgress += MOVE_SPEED * dt;
    if (m_moveProgress >= 1.0f) {
        m_moveProgress = 1.0f;
        m_pixelX = static_cast<float>(m_targetX);
        m_pixelY = static_cast<float>(m_targetY);
        m_isMoving = false;
    } else {
        // Lerp from previous logical position toward target
        const float startX = static_cast<float>(m_x);
        const float startY = static_cast<float>(m_y);
        m_pixelX = startX + (static_cast<float>(m_targetX) - startX) * m_moveProgress;
        m_pixelY = startY + (static_cast<float>(m_targetY) - startY) * m_moveProgress;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// move — initiates movement to adjacent tile
// ─────────────────────────────────────────────────────────────────────────────
void Player::move(Direction dir, const Level& level)
{
    int newX = m_targetX;
    int newY = m_targetY;

    switch (dir) {
    case Direction::None:
        return;
    case Direction::Up:
        newY -= 1;
        m_animState = AnimationState::MovingUp;
        m_lastDir   = AnimationState::MovingUp;
        break;
    case Direction::Down:
        newY += 1;
        m_animState = AnimationState::MovingDown;
        m_lastDir   = AnimationState::MovingDown;
        break;
    case Direction::Left:
        newX -= 1;
        m_animState = AnimationState::MovingLeft;
        m_lastDir   = AnimationState::MovingLeft;
        break;
    case Direction::Right:
        newX += 1;
        m_animState = AnimationState::MovingRight;
        m_lastDir   = AnimationState::MovingRight;
        break;
    }

    if (!level.isWalkable(newX, newY)) {
        m_animState = AnimationState::Idle;
        return;
    }

    // Update logical position immediately (for collision/interaction logic)
    setPosition(newX, newY);

    // Set up smooth interpolation: slide from current pixel pos to new tile
    m_targetX = newX;
    m_targetY = newY;
    m_isMoving = true;
    m_moveProgress = 0.0f;

    // Reset animation frame at start of new step
    m_animFrame = 0;
    m_animTick  = ANIM_TICKS_PER_FRAME;
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

AnimationState Player::animState() const
{
    return m_animState;
}

int Player::animFrame() const
{
    return m_animFrame;
}

// ─────────────────────────────────────────────────────────────────────────────
// advanceAnimation — called each render tick (16 ms)
// ─────────────────────────────────────────────────────────────────────────────
void Player::advanceAnimation()
{
    if (m_animState == AnimationState::Idle) {
        m_animFrame = 0;
        return;
    }

    if (!m_isMoving) {
        // We arrived at the tile — go back to idle
        m_animState = AnimationState::Idle;
        m_animFrame = 0;
        return;
    }

    // Determine clip length
    int clipLen = 4;
    if (m_sprites) {
        AnimationState drawState = (m_animState == AnimationState::Idle) ? m_lastDir : m_animState;
        QString clipName;
        switch (drawState) {
        case AnimationState::MovingUp:    clipName = "player_move_up";    break;
        case AnimationState::MovingDown:  clipName = "player_move_down";  break;
        case AnimationState::MovingLeft:  clipName = "player_move_left";  break;
        case AnimationState::MovingRight: clipName = "player_move_right"; break;
        default:                          clipName = "player_move_down";  break;
        }
        if (const AnimationClip* c = m_sprites->clip(clipName)) {
            clipLen = c->frameCount;
        }
    }

    if (--m_animTick <= 0) {
        m_animTick  = ANIM_TICKS_PER_FRAME;
        m_animFrame = (m_animFrame + 1) % clipLen;
    }
}

void Player::setSpriteManager(const SpriteManager* sm)
{
    m_sprites = sm;
}
