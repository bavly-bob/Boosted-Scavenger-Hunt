#pragma once

#include "Enums.h"

#include <QString>

class SpriteManager;
struct AnimationClip;

class PlayerAnimator {
public:
    explicit PlayerAnimator();

    void notifyMoveStarted(AnimationState dir);
    void notifyArrived();

    void tick(const SpriteManager* sprites);

    AnimationState animState()  const { return m_animState; }
    AnimationState lastDir()    const { return m_lastDir;   }
    int            animFrame()  const { return m_animFrame; }

    static QString clipName(AnimationState state);

private:
    AnimationState m_animState;
    AnimationState m_lastDir;
    int            m_animFrame;
    int            m_animTick;

    static constexpr int DEFAULT_CLIP_LEN      = 4;
    static constexpr int ANIM_TICKS_PER_FRAME  = 4;
};
