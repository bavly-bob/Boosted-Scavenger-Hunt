#include "SpriteManager.h"

namespace {
QPixmap kEmptyPixmap;
} // namespace

const QPixmap& SpriteManager::sprite(const QString& key) const
{
    const auto it = m_cache.constFind(key);
    if (it == m_cache.constEnd()) {
        return kEmptyPixmap;
    }
    return it.value();
}

void SpriteManager::load(const QString& key, const QString& filePath)
{
    QPixmap pix(filePath);
    m_cache.insert(key, pix);
}

void SpriteManager::clear()
{
    m_cache.clear();
}

