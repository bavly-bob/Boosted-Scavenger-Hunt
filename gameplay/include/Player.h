#pragma once

#include "Enums.h"
#include "GameObject.h"
#include "PlayerAnimator.h"

class Level;
class QPainter;
class SpriteManager;

class Player : public GameObject {
    int  m_coinsCollected;

    // ── Smooth movement ──────────────────────────────────────────────────────
    // Logical tile target (where the player will end up after this step)
    int   m_targetX;
    int   m_targetY;

    // Sub-tile visual position (in pixels, in tile-space units)
    float m_pixelX;
    float m_pixelY;

    // Previous pixel position at the start of the current step (lerp origin).
    // Stored separately so updateMovement interpolates from where the sprite
    // actually was, not from the already-snapped logical position.
    float m_prevPixelX;
    float m_prevPixelY;

    bool  m_isMoving;
    float m_moveProgress; // 0.0 → 1.0

    // ── Animation ────────────────────────────────────────────────────────────
    PlayerAnimator           m_animator;
    const SpriteManager*     m_sprites; // borrowed, never owned

    static constexpr float MOVE_SPEED = 8.0f; // tiles per second

public:
    explicit Player(int x, int y);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    // Update smooth movement. dt = seconds since last frame.
    void updateMovement(float dt);

    void move(Direction dir, const Level& level);
    void collectCoin();
    int  getCoinsCollected() const;
    bool canEnterTreasureRoom() const;

    // Returns true when the player has fully arrived at the target tile.
    bool isAtTarget() const;

    // ── Animation (delegated to PlayerAnimator) ───────────────────────────────
    // Called once per render tick (≈16 ms) by GameWindow's render timer.
    void advanceAnimation();

    AnimationState animState() const;
    int            animFrame() const;

    // Inject sprite manager (borrowed, not owned).
    void setSpriteManager(const SpriteManager* sm);
};
