#pragma once

#include "Wall.h"

#include <QSet>
#include <QVector>

class QPainter;

class HiddenWall : public Wall {
    bool m_open;
    int m_wallId;
    int m_connectedNodeA;
    int m_connectedNodeB;
    QSet<int> m_requiredTriggerIds;
    QSet<int> m_activatedTriggerIds;

public:
    HiddenWall(int x, int y, int wallId = -1, int connectedNodeA = -1, int connectedNodeB = -1);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    bool isBlocking() const override;

    void open();
    bool isOpen() const;

    int wallId() const;
    void setWallId(int wallId);

    int connectedNodeA() const;
    int connectedNodeB() const;

    QVector<int> requiredTriggerIds() const;
    bool hasRequiredTriggers() const;
    void setRequiredTriggerIds(const QVector<int>& triggerIds);
    void addRequiredTriggerId(int triggerId);
    bool onTriggerActivated(int triggerId);

    // Backward compatibility accessors.
    int triggerId() const;
    void setTriggerId(int triggerId);
};
