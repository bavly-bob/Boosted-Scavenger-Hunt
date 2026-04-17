#pragma once

#include "Wall.h"

#include <QPoint>
#include <QVector>

class QPainter;

class TriggerWall : public Wall {
    QVector<QPoint> m_opensPositions;
    bool m_triggered;

public:
    TriggerWall(int x, int y, const QVector<QPoint>& opens);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;
    bool isBlocking() const override;

    void trigger();
    bool isTriggered() const;
    QVector<QPoint> getOpensPositions() const;
};
