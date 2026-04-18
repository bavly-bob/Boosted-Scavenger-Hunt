#pragma once

#include "Enums.h"
#include "GameObject.h"

class Level;
class Player;
class QPainter;
class SpriteManager;

class Enemy : public GameObject {
    AnimationState m_animState;
    int            m_animFrame;
    int            m_animTick;

    int m_moveTimer;
    int m_moveInterval;

    const SpriteManager* m_sprites;

public:
    explicit Enemy(int x, int y);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    void update(Level& level, const Player& player);
    void advanceAnimation();
    void setSpriteManager(const SpriteManager* sm);

    AnimationState animState() const { return m_animState; }
    int animFrame() const { return m_animFrame; }

    void setDying();
    bool isDead() const;
};
