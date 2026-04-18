#pragma once

#include "GameObject.h"

class QPainter;
class SpriteManager;

class Wall : public GameObject {
    int m_tileVariant;
    const SpriteManager* m_sprites;

public:
    explicit Wall(int x, int y, int tileVariant = 0);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    virtual bool isBlocking() const;

    int  getTileVariant() const;
    void setTileVariant(int v);

    void setSpriteManager(const SpriteManager* sm);

protected:
    const SpriteManager* spriteManager() const { return m_sprites; }
};
