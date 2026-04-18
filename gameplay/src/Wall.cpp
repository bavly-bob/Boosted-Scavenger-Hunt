#include "Wall.h"

#include "SpriteManager.h"

#include <QPainter>

Wall::Wall(int x, int y, int tileVariant)
    : GameObject(x, y),
      m_tileVariant(tileVariant),
      m_sprites(nullptr)
{
}

QString Wall::getType() const
{
    return "Wall";
}

void Wall::draw(QPainter& painter, int cellSize) const
{
    const QRect r(m_x * cellSize, m_y * cellSize, cellSize, cellSize);

    if (m_sprites) {
        const QString key = QString("wall_%1").arg(m_tileVariant);
        const QPixmap& pix = m_sprites->sprite(key);
        if (!pix.isNull()) {
            painter.drawPixmap(r, pix);
            return;
        }
    }

    painter.save();

    // Keep wall variants visually distinct when sprite assets are missing.
    static const QColor variantColors[] = {
        QColor(72, 72, 84),
        QColor(60, 80, 60),
        QColor(90, 70, 55),
        QColor(55, 55, 75),
    };
    const int numVariants = static_cast<int>(sizeof(variantColors) / sizeof(variantColors[0]));
    const QColor base = variantColors[m_tileVariant % numVariants];

    painter.setPen(Qt::NoPen);
    painter.setBrush(base);
    painter.drawRect(r);

    painter.setPen(QPen(base.lighter(140), 1));
    painter.drawLine(r.topLeft(), r.topRight());
    painter.drawLine(r.topLeft(), r.bottomLeft());

    painter.setPen(QPen(base.darker(160), 1));
    painter.drawLine(r.bottomLeft(), r.bottomRight());
    painter.drawLine(r.topRight(), r.bottomRight());

    painter.setPen(QPen(base.darker(130), 1));
    const int halfH = r.top() + cellSize / 2;
    painter.drawLine(r.left() + 1, halfH, r.right() - 1, halfH);

    // Stagger vertical joints by row to mimic a brick pattern.
    const int jX = (m_y % 2 == 0) ? r.left() + cellSize / 2 : r.left() + cellSize / 4;
    painter.drawLine(jX, r.top() + 1, jX, halfH - 1);
    const int jX2 = jX + cellSize / 2;
    if (jX2 < r.right()) {
        painter.drawLine(jX2, halfH + 1, jX2, r.bottom() - 1);
    }

    painter.restore();
}

bool Wall::isBlocking() const
{
    return true;
}

int Wall::getTileVariant() const
{
    return m_tileVariant;
}

void Wall::setTileVariant(int v)
{
    m_tileVariant = v;
}

void Wall::setSpriteManager(const SpriteManager* sm)
{
    m_sprites = sm;
}
