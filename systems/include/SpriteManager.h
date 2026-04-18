#pragma once

#include "Enums.h"
#include "Enemy.h"

#include <QHash>
#include <QPixmap>
#include <QRect>
#include <QString>

class QPainter;
class Player;
class Wall;

struct AnimationClip {
    QString spriteKey;
    int     frameCount;
    int     frameWidth;
    int     frameHeight;
    int     srcY;
    int     fps;
};

class SpriteManager {
public:
    const QPixmap& sprite(const QString& key) const;
    void           load(const QString& key, const QString& filePath);
    void           loadPixmap(const QString& key, const QPixmap& pixmap);
    void           clear();

    void                registerClip(const QString& name, const AnimationClip& clip);
    const AnimationClip* clip(const QString& name) const;

    int ticksPerFrame(const QString& clipName) const;

    bool drawWall(const Wall& wall, QPainter& p, int cellSize) const;
    bool drawPlayer(const Player& player, QPainter& p, int cellSize) const;
    bool drawEnemy(const Enemy& enemy, QPainter& p, int cellSize) const;

private:
    mutable QHash<QString, QPixmap> m_cache;
    QHash<QString, AnimationClip>   m_clips;

    static QString clipNameForState(AnimationState state);
};
