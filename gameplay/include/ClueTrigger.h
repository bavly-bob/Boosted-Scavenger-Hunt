#pragma once

#include "GameObject.h"

class QPainter;

class ClueTrigger : public GameObject {
    QString m_clueText;
    bool m_activated;

public:
    ClueTrigger(int x, int y, const QString& text);

    QString getType() const override;
    void draw(QPainter& painter, int cellSize) const override;

    void activate();
    bool isActivated() const;
    QString getClueText() const;
};

