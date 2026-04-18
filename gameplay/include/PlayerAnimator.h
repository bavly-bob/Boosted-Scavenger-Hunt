#pragma once

#include "Enums.h"

#include <QString>

class SpriteManager;
struct AnimationClip;

// ─────────────────────────────────────────────────────────────────────────────
// PlayerAnimator
//
// Owns all animation state for the player:
//   - current AnimationState (Idle / MovingUp / Down / Left / Right)
//   - last facing direction (persists when Idle, used for the draw clip)
//   - current frame index and countdown tick
//
// Call notifyMoveStarted(dir)  when the player begins moving.
// Call notifyArrived()         when the player reaches the target tile.
// Call tick()                  once per render frame (≈16 ms) to advance frames.
//
// Query drawState() + drawFrame() for rendering.
// ─────────────────────────────────────────────────────────────────────────────
class PlayerAnimator {
public:
    explicit PlayerAnimator();

    // ── State transitions ────────────────────────────────────────────────────
    void notifyMoveStarted(AnimationState dir);
    void notifyArrived();

    // ── Per-render-frame tick ────────────────────────────────────────────────
    // Advances the frame counter. Pass the optional sprite manager to pick the
    // correct clip length; if nullptr, falls back to a default of 4 frames.
    void tick(const SpriteManager* sprites);

    // ── Read-only accessors (for draw + Player::animState/Frame) ─────────────
    AnimationState animState()  const { return m_animState; }
    AnimationState lastDir()    const { return m_lastDir;   }
    int            animFrame()  const { return m_animFrame; }

    // The clip name to use for rendering (resolves Idle → last-dir clip).
    static QString clipName(AnimationState state);

private:
    AnimationState m_animState;
    AnimationState m_lastDir;
    int            m_animFrame;
    int            m_animTick;

    static constexpr int DEFAULT_CLIP_LEN      = 4;
    static constexpr int ANIM_TICKS_PER_FRAME  = 4; // render ticks per walk frame
};
