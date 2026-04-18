#include "PlayerAnimator.h"

#include "SpriteManager.h"

#include <QString>

PlayerAnimator::PlayerAnimator()
    : m_animState(AnimationState::Idle),
      m_lastDir(AnimationState::MovingDown),
      m_animFrame(0),
      m_animTick(ANIM_TICKS_PER_FRAME)
{
}

void PlayerAnimator::notifyMoveStarted(AnimationState dir)
{
    m_animState = dir;
    m_lastDir   = dir;
    m_animFrame = 0;
    m_animTick  = ANIM_TICKS_PER_FRAME;
}

void PlayerAnimator::notifyArrived()
{
    m_animState = AnimationState::Idle;
    m_animFrame = 0;
}

void PlayerAnimator::tick(const SpriteManager* sprites)
{
    if (m_animState == AnimationState::Idle) {
        m_animFrame = 0;
        return;
    }

    int clipLen = DEFAULT_CLIP_LEN;
    if (sprites) {
        const QString name = clipName(m_animState);
        if (const AnimationClip* c = sprites->clip(name)) {
            clipLen = c->frameCount;
        }
    }

    if (--m_animTick <= 0) {
        m_animTick  = ANIM_TICKS_PER_FRAME;
        m_animFrame = (m_animFrame + 1) % clipLen;
    }
}

QString PlayerAnimator::clipName(AnimationState state)
{
    switch (state) {
    case AnimationState::MovingUp:    return QStringLiteral("player_move_up");
    case AnimationState::MovingDown:  return QStringLiteral("player_move_down");
    case AnimationState::MovingLeft:  return QStringLiteral("player_move_left");
    case AnimationState::MovingRight: return QStringLiteral("player_move_right");
    default:                          return QStringLiteral("player_move_down");
    }
}
