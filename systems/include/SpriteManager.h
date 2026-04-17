#pragma once

#include "Enums.h"

#include <QHash>
#include <QPixmap>
#include <QRect>
#include <QString>

class QPainter;
class Player;
class Wall;

// ─────────────────────────────────────────────────────────────────────────────
// AnimationClip
//   Describes a horizontal sprite strip where each frame is frameWidth × frameHeight px.
// ─────────────────────────────────────────────────────────────────────────────
struct AnimationClip {
    QString spriteKey;   // key into the SpriteManager cache
    int     frameCount;  // total frames in the strip
    int     frameWidth;  // px width of one frame
    int     frameHeight; // px height of one frame
    int     fps;         // logical frames per second (used by advanceAnimation)
};

// ─────────────────────────────────────────────────────────────────────────────
// SpriteManager
//   Sprite cache + animation clip registry + high-level draw helpers.
//   All rendering details live here; gameplay entities only call drawWall /
//   drawPlayer and never touch raw pixel arithmetic themselves.
// ─────────────────────────────────────────────────────────────────────────────
class SpriteManager {
public:
    // ── Cache ───────────────────────────────────────────────────────────────
    const QPixmap& sprite(const QString& key) const;
    void           load(const QString& key, const QString& filePath);
    void           loadPixmap(const QString& key, const QPixmap& pixmap); // insert pre-built pixmap
    void           clear();

    // ── Animation clips ─────────────────────────────────────────────────────
    void                registerClip(const QString& name, const AnimationClip& clip);
    const AnimationClip* clip(const QString& name) const;

    // Total number of ticks between frame advances for a named clip.
    // Returns 1 if the clip is not found (advance every tick).
    int ticksPerFrame(const QString& clipName) const;

    // ── Draw helpers ────────────────────────────────────────────────────────
    // Renders a wall tile at its grid position.
    // Falls back silently if no sprite is loaded (caller handles fallback).
    bool drawWall(const Wall& wall, QPainter& p, int cellSize) const;

    // Renders the player at its grid position using the correct animation frame.
    bool drawPlayer(const Player& player, QPainter& p, int cellSize) const;

private:
    mutable QHash<QString, QPixmap>       m_cache;
    QHash<QString, AnimationClip>          m_clips;

    // Map AnimationState → clip name
    static QString clipNameForState(AnimationState state);
};
