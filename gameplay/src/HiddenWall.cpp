#include "HiddenWall.h"

#include <QPainter>
#include <algorithm>

HiddenWall::HiddenWall(int x, int y, int wallId, int connectedNodeA, int connectedNodeB)
    : Wall(x, y),
      m_open(false),
      m_wallId(wallId),
      m_connectedNodeA(connectedNodeA),
      m_connectedNodeB(connectedNodeB)
{
}

QString HiddenWall::getType() const
{
    return "HiddenWall";
}

void HiddenWall::draw(QPainter& painter, int cellSize) const
{
    if (m_open) {
        return;
    }
    // Hidden walls intentionally render as normal walls until triggered.
    Wall::draw(painter, cellSize);
}

bool HiddenWall::isBlocking() const
{
    return !m_open;
}

void HiddenWall::open()
{
    m_open = true;
}

bool HiddenWall::isOpen() const
{
    return m_open;
}

int HiddenWall::wallId() const
{
    return m_wallId;
}

void HiddenWall::setWallId(int wallId)
{
    m_wallId = wallId;
}

int HiddenWall::connectedNodeA() const
{
    return m_connectedNodeA;
}

int HiddenWall::connectedNodeB() const
{
    return m_connectedNodeB;
}

QVector<int> HiddenWall::requiredTriggerIds() const
{
    QVector<int> ids = m_requiredTriggerIds.values().toVector();
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool HiddenWall::hasRequiredTriggers() const
{
    return !m_requiredTriggerIds.isEmpty();
}

void HiddenWall::setRequiredTriggerIds(const QVector<int>& triggerIds)
{
    m_requiredTriggerIds.clear();
    m_activatedTriggerIds.clear();
    for (int id : triggerIds) {
        if (id >= 0) {
            m_requiredTriggerIds.insert(id);
        }
    }
}

void HiddenWall::addRequiredTriggerId(int triggerId)
{
    if (triggerId >= 0) {
        m_requiredTriggerIds.insert(triggerId);
    }
}

bool HiddenWall::onTriggerActivated(int triggerId)
{
    if (m_open || triggerId < 0 || !m_requiredTriggerIds.contains(triggerId)) {
        return false;
    }
    m_activatedTriggerIds.insert(triggerId);
    if (m_activatedTriggerIds.size() >= m_requiredTriggerIds.size()) {
        m_open = true;
        return true;
    }
    return false;
}

int HiddenWall::triggerId() const
{
    if (m_requiredTriggerIds.isEmpty()) {
        return -1;
    }
    return *std::min_element(m_requiredTriggerIds.begin(), m_requiredTriggerIds.end());
}

void HiddenWall::setTriggerId(int triggerId)
{
    if (triggerId < 0) {
        m_requiredTriggerIds.clear();
        m_activatedTriggerIds.clear();
        return;
    }
    m_requiredTriggerIds = QSet<int>{triggerId};
    m_activatedTriggerIds.clear();
}
