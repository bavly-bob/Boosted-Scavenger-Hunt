#pragma once

#include <QHash>
#include <QPixmap>
#include <QString>

// Simple sprite cache. In Phase 4/5 we can map tile/object types to sprite keys
// and draw QPixmaps instead of procedural shapes.
class SpriteManager {
public:
    const QPixmap& sprite(const QString& key) const;
    void load(const QString& key, const QString& filePath);
    void clear();

private:
    mutable QHash<QString, QPixmap> m_cache;
};

