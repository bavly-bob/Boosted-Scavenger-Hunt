#include "GameOverOverlay.h"

#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

GameOverOverlay::GameOverOverlay(QWidget *parent)
    : QWidget(parent),
      m_titleLabel(new QLabel(this)),
      m_scoreLabel(new QLabel(this)),
      m_nextLevelButton(new QPushButton("Next Level", this)),
      m_restartButton(new QPushButton("Restart Level", this)),
      m_mainMenuButton(new QPushButton("Main Menu", this))
{
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(80, 90, 80, 90);
    layout->setSpacing(14);

    layout->addStretch(1);

    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("color: rgba(240,235,225,0.95); font-size: 34px; font-weight: 900; letter-spacing: 2px;");
    layout->addWidget(m_titleLabel);

    m_scoreLabel->setAlignment(Qt::AlignCenter);
    m_scoreLabel->setStyleSheet("color: rgba(220,215,205,0.85); font-size: 14px; font-weight: 600;");
    layout->addWidget(m_scoreLabel);

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

    styleButton(m_nextLevelButton, "rgba(80,200,120,0.30)");
    styleButton(m_restartButton, "rgba(230,180,60,0.30)");
    styleButton(m_mainMenuButton, "rgba(220,80,70,0.30)");

    layout->addSpacing(12);
    layout->addWidget(m_nextLevelButton);
    layout->addWidget(m_restartButton);
    layout->addWidget(m_mainMenuButton);
    layout->addStretch(2);

    connect(m_nextLevelButton, &QPushButton::clicked, this, &GameOverOverlay::nextLevelRequested);
    connect(m_restartButton, &QPushButton::clicked, this, &GameOverOverlay::restartRequested);
    connect(m_mainMenuButton, &QPushButton::clicked, this, &GameOverOverlay::mainMenuRequested);
}

void GameOverOverlay::setResult(bool won, int score, bool hasNextLevel)
{
    m_titleLabel->setText(won ? "YOU WIN" : "GAME OVER");
    m_scoreLabel->setText(QString("Score: %1").arg(score));
    m_nextLevelButton->setVisible(hasNextLevel);
}

void GameOverOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 185));
}

