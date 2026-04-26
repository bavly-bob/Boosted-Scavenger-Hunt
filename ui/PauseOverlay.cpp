#include "PauseOverlay.h"

#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QKeyEvent>

PauseOverlay::PauseOverlay(QWidget *parent)
    : QWidget(parent),
      m_resumeButton(new QPushButton("Resume", this)),
      m_restartButton(new QPushButton("Restart Level", this)),
      m_mainMenuButton(new QPushButton("Main Menu", this))
{
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(80, 90, 80, 90);
    layout->setSpacing(14);

    layout->addStretch(1);

    auto *title = new QLabel("PAUSED", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color: rgba(240,235,225,0.95); font-size: 34px; font-weight: 800; letter-spacing: 2px;");
    layout->addWidget(title);

    auto styleButton = [](QPushButton* b, const QString& bg) {
        b->setMinimumHeight(44);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString(
            "QPushButton { background: %1; color: rgba(245,240,230,0.95); border: 1.5px solid rgba(255,255,255,0.20);"
            "  border-radius: 12px; font-size: 15px; font-weight: 650; }"
            "QPushButton:hover { border-color: rgba(255,255,255,0.45); }"
            "QPushButton:pressed { background: rgba(0,0,0,0.25); padding-top: 2px; }"
        ).arg(bg));
    };

    styleButton(m_resumeButton, "rgba(80,200,120,0.30)");
    styleButton(m_restartButton, "rgba(230,180,60,0.30)");
    styleButton(m_mainMenuButton, "rgba(220,80,70,0.30)");

    layout->addSpacing(10);
    layout->addWidget(m_resumeButton);
    layout->addWidget(m_restartButton);
    layout->addWidget(m_mainMenuButton);
    layout->addStretch(2);

    connect(m_resumeButton, &QPushButton::clicked, this, &PauseOverlay::resumeRequested);
    connect(m_restartButton, &QPushButton::clicked, this, &PauseOverlay::restartRequested);
    connect(m_mainMenuButton, &QPushButton::clicked, this, &PauseOverlay::mainMenuRequested);
}

void PauseOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 170));
}

void PauseOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_resumeButton->setFocus();
}

void PauseOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit resumeRequested();
        return;
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_resumeButton->hasFocus()) emit resumeRequested();
        else if (m_restartButton->hasFocus()) emit restartRequested();
        else if (m_mainMenuButton->hasFocus()) emit mainMenuRequested();
        return;
    }
    QWidget::keyPressEvent(event);
}

