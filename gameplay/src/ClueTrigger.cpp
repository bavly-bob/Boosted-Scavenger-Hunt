#include "ClueTrigger.h"

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

void ClueTrigger::draw(QPainter&, int) const
{
    // Intentionally invisible.
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

