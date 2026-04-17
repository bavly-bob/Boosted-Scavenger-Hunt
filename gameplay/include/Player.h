#pragma once

#include "Enums.h"
#include "GameObject.h"

class Level;
class QPainter;
class SpriteManager;

class Player : public GameObject {
    int            m_coinsCollected;

    // Animation state
    AnimationState m_animState;
    int            m_animFrame;   // current frame index within the clip
    int            m_animTick;    // countdown ticks until next frame advance

    const SpriteManager* m_sprites; // optional, borrowed pointer (may be nullptr)

public:
    explicit Player(int x, int y);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    void move(Direction dir, const Level& level);
    void collectCoin();
    int  getCoinsCollected() const;
    bool canEnterTreasureRoom() const;

    // Animation
    AnimationState animState()  const;
    int            animFrame()  const;
    // Called once per game tick to advance the animation frame.
    void advanceAnimation();

    // Inject sprite manager (borrowed, not owned).
    void setSpriteManager(const SpriteManager* sm);
};
