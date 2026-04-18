#pragma once

#include "Enums.h"
#include "GameObject.h"
#include "PlayerAnimator.h"

class Level;
class QPainter;
class SpriteManager;

class Player : public GameObject {
    int  m_coinsCollected;

    int   m_targetX;
    int   m_targetY;

    float m_pixelX;
    float m_pixelY;

    // Lerp origin captured when movement starts.
    float m_prevPixelX;
    float m_prevPixelY;

    bool  m_isMoving;
    float m_moveProgress;

    PlayerAnimator       m_animator;
    const SpriteManager* m_sprites;

    static constexpr float MOVE_SPEED = 8.0f;

public:
    explicit Player(int x, int y);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    void updateMovement(float dt);

    void move(Direction dir, const Level& level);
    void collectCoin();
    int  getCoinsCollected() const;
    bool canEnterTreasureRoom() const;

    bool isAtTarget() const;

    void advanceAnimation();

    AnimationState animState() const;
    int            animFrame() const;

    void setSpriteManager(const SpriteManager* sm);
};
