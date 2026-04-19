#include "TriggerWall.h"

#include "SpriteManager.h"

#include <QPainter>
#include <algorithm>

TriggerWall::TriggerWall(int x, int y, int triggerId, const QVector<int>& controlledWallIds)
    : Wall(x, y),
      m_controlledWallIds(controlledWallIds),
      m_triggered(false),
      m_triggerId(triggerId)
{
    std::sort(m_controlledWallIds.begin(), m_controlledWallIds.end());
    m_controlledWallIds.erase(std::unique(m_controlledWallIds.begin(), m_controlledWallIds.end()),
                              m_controlledWallIds.end());
}

QString TriggerWall::getType() const
{
    return "TriggerWall";
}

void TriggerWall::draw(QPainter& painter, int cellSize) const
{
    const QRect r(m_x * cellSize, m_y * cellSize, cellSize, cellSize);

    if (const SpriteManager* sm = spriteManager()) {
        const QString key = m_triggered
                            ? QStringLiteral("pressure_plate_on")
                            : QStringLiteral("pressure_plate");
        const QPixmap& pix = sm->sprite(key);
        if (!pix.isNull()) {
            painter.drawPixmap(r, pix);
            return;
        }
    }

    painter.save();
    painter.setPen(QPen(QColor(45, 45, 50), 1));
    painter.setBrush(QColor(70, 70, 80));
    painter.drawRect(r.adjusted(1, 1, -1, -1));

    painter.setPen(QPen(m_triggered ? QColor(80, 200, 120) : QColor(230, 180, 60), 2));
    painter.drawRect(r.adjusted(4, 4, -4, -4));
    painter.restore();
}

bool TriggerWall::isBlocking() const
{
    return false;
}

void TriggerWall::trigger()
{
    m_triggered = true;
}

bool TriggerWall::isTriggered() const
{
    return m_triggered;
}

QVector<int> TriggerWall::controlledWallIds() const
{
    return m_controlledWallIds;
}

int TriggerWall::triggerId() const
{
    return m_triggerId;
}
