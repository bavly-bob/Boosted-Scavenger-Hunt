#pragma once

#include "Enums.h"
#include "GameObject.h"

class Level;
class QPainter;
class SpriteManager;

class Player : public GameObject {
    int            m_coinsCollected;

    // Logical tile target (where the player will end up)
    int m_targetX;
    int m_targetY;

    // Sub-tile visual position (in pixels, used for smooth movement)
    float m_pixelX;
    float m_pixelY;

    // Are we currently interpolating between tiles?
    bool  m_isMoving;
    float m_moveProgress; // 0.0 -> 1.0

    // Animation state
    AnimationState m_animState;
    int            m_animFrame;   // current frame index within the clip
    int            m_animTick;    // countdown ticks until next frame advance
    AnimationState m_lastDir;     // facing direction (persists when idle)

    const SpriteManager* m_sprites; // optional, borrowed pointer (may be nullptr)

    static constexpr float MOVE_SPEED = 8.0f; // tiles per second
    static constexpr int   ANIM_TICKS_PER_FRAME = 4; // render ticks per walk frame

public:
    explicit Player(int x, int y);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    // Update smooth movement. dt = seconds since last frame.
    void updateMovement(float dt, int tileSize);

    void move(Direction dir, const Level& level);
    void collectCoin();
    int  getCoinsCollected() const;
    bool canEnterTreasureRoom() const;

    // Returns true if the player has arrived at target tile (logic-safe to process next input).
    bool isAtTarget() const;

    // Animation
    AnimationState animState()  const;
    int            animFrame()  const;
    // Called once per render tick to advance the animation frame.
    void advanceAnimation();

    // Inject sprite manager (borrowed, not owned).
    void setSpriteManager(const SpriteManager* sm);
};
