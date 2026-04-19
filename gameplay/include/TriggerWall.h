#pragma once

#include "Wall.h"

#include <QVector>

class QPainter;

class TriggerWall : public Wall {
    QVector<int> m_controlledWallIds;
    bool m_triggered;
    int m_triggerId;

public:
    TriggerWall(int x, int y, int triggerId, const QVector<int>& controlledWallIds);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;
    bool isBlocking() const override;

    void trigger();
    bool isTriggered() const;
    QVector<int> controlledWallIds() const;
    int triggerId() const;
};
