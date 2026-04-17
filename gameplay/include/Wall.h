#pragma once

#include "GameObject.h"

class QPainter;
class SpriteManager;

// Wall tile on the level grid.
// m_tileVariant selects which sprite/visual variant is used for rendering.
// The logic (isBlocking) is independent of the visual variant.
class Wall : public GameObject {
    int  m_tileVariant;          // 0 = default stone, 1 = mossy, 2 = brick, etc.
    const SpriteManager* m_sprites; // optional, borrowed pointer (may be nullptr)

public:
    explicit Wall(int x, int y, int tileVariant = 0);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    virtual bool isBlocking() const;

    int  getTileVariant() const;
    void setTileVariant(int v);

    // Inject a SpriteManager so draw() can use textured rendering.
    // The Wall does NOT own the SpriteManager.
    void setSpriteManager(const SpriteManager* sm);
};
