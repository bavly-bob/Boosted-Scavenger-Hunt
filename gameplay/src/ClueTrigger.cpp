#include "ClueTrigger.h"

#include <QPainter>
#include <QRect>
#include <QFont>

ClueTrigger::ClueTrigger(int x, int y, const QString& text)
    : GameObject(x, y),
      m_clueText(text),
      m_activated(false)
{
}

QString ClueTrigger::getType() const
{
    return "ClueTrigger";
}

void ClueTrigger::draw(QPainter& painter, int tileSize) const
{
    // instead of nothing, draw a question mark icon
    // if the clue is activated, do not draw the question mark icon
    if (!m_activated) {
        painter.save();
        QRect rect(m_x * tileSize, m_y * tileSize, tileSize, tileSize);
        painter.setFont(QFont("Arial", tileSize / 2, QFont::Bold));
        painter.setPen(Qt::yellow);
        painter.drawText(rect, Qt::AlignCenter, "?");
        painter.restore();
    }
}

void ClueTrigger::activate()
{
    m_activated = true;
}

bool ClueTrigger::isActivated() const
{
    return m_activated;
}

QString ClueTrigger::getClueText() const
{
    return m_clueText;
}

