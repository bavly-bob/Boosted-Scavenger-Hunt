#include "SpriteManager.h"

#include "Player.h"
#include "Wall.h"
#include "Enemy.h"

#include <QPainter>
#include <QRect>

// ── Internal helpers ──────────────────────────────────────────────────────────
namespace {
const QPixmap& emptyPixmap()
{
    static QPixmap empty;
    return empty;
}
} // namespace

// ── Cache ─────────────────────────────────────────────────────────────────────

const QPixmap& SpriteManager::sprite(const QString& key) const
{
    const auto it = m_cache.constFind(key);
    if (it == m_cache.constEnd()) {
        return emptyPixmap();
    }
    return it.value();
}

void SpriteManager::load(const QString& key, const QString& filePath)
{
    QPixmap pix(filePath);
    m_cache.insert(key, pix);
}

void SpriteManager::loadPixmap(const QString& key, const QPixmap& pixmap)
{
    m_cache.insert(key, pixmap);
}


void SpriteManager::clear()
{
    m_cache.clear();
    m_clips.clear();
}

// ── Animation clips ───────────────────────────────────────────────────────────

void SpriteManager::registerClip(const QString& name, const AnimationClip& clip)
{
    m_clips.insert(name, clip);
}

const AnimationClip* SpriteManager::clip(const QString& name) const
{
    const auto it = m_clips.constFind(name);
    if (it == m_clips.constEnd()) {
        return nullptr;
    }
    return &it.value();
}

int SpriteManager::ticksPerFrame(const QString& clipName) const
{
    if (const AnimationClip* c = clip(clipName)) {
        // Assume game tick = ~16 ms (60 fps timer); derive ticks from clip fps.
        const int ticks = qMax(1, 60 / qMax(1, c->fps));
        return ticks;
    }
    return 1;
}

// ── Static helper ─────────────────────────────────────────────────────────────

QString SpriteManager::clipNameForState(AnimationState state)
{
    switch (state) {
    case AnimationState::MovingUp:    return QStringLiteral("player_move_up");
    case AnimationState::MovingDown:  return QStringLiteral("player_move_down");
    case AnimationState::MovingLeft:  return QStringLiteral("player_move_left");
    case AnimationState::MovingRight: return QStringLiteral("player_move_right");
    default:                          return QStringLiteral("player_idle");
    }
}

// ── Draw helpers ──────────────────────────────────────────────────────────────

bool SpriteManager::drawWall(const Wall& wall, QPainter& p, int cellSize) const
{
    const QString key = QString("wall_%1").arg(wall.getTileVariant());
    const QPixmap& pix = sprite(key);
    if (pix.isNull()) {
        return false; // caller should use procedural fallback
    }
    const QRect r(wall.getX() * cellSize, wall.getY() * cellSize, cellSize, cellSize);
    p.drawPixmap(r, pix);
    return true;
}

bool SpriteManager::drawPlayer(const Player& player, QPainter& p, int cellSize) const
{
    const QString cName = clipNameForState(player.animState());
    const AnimationClip* c = clip(cName);
    if (!c) {
        return false; // no clip registered → use procedural fallback
    }

    const QPixmap& sheet = sprite(c->spriteKey);
    if (sheet.isNull()) {
        return false;
    }

    const int frame = player.animFrame() % c->frameCount;
    const QRect srcRect(frame * c->frameWidth, c->srcY, c->frameWidth, c->frameHeight);
    QRect dstRect(player.getX() * cellSize, player.getY() * cellSize, cellSize, cellSize);
    dstRect.adjust(-4, -4, 4, 4); // slightly larger, overlapping edges
    p.drawPixmap(dstRect, sheet, srcRect);
    return true;
}

bool SpriteManager::drawEnemy(const Enemy& enemy, QPainter& p, int cellSize) const
{
    QString cName = "enemy_idle";
    switch (enemy.animState()) {
    case AnimationState::MovingUp:    cName = "enemy_move_up";    break;
    case AnimationState::MovingDown:  cName = "enemy_move_down";  break;
    case AnimationState::MovingLeft:  cName = "enemy_move_left";  break;
    case AnimationState::MovingRight: cName = "enemy_move_right"; break;
    case AnimationState::Dying:       cName = "enemy_die";        break;
    default:                          cName = "enemy_idle";       break;
    }

    const AnimationClip* c = clip(cName);
    if (!c) {
        return false;
    }

    const QPixmap& sheet = sprite(c->spriteKey);
    if (sheet.isNull()) {
        return false;
    }

    const int frame = enemy.animFrame() % c->frameCount;
    const QRect srcRect(frame * c->frameWidth, c->srcY, c->frameWidth, c->frameHeight);
    QRect dstRect(enemy.getX() * cellSize, enemy.getY() * cellSize, cellSize, cellSize);
    dstRect.adjust(-4, -4, 4, 4); // slightly larger
    p.drawPixmap(dstRect, sheet, srcRect);
    return true;
}
