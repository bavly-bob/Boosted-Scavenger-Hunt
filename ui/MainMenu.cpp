#include "MainMenu.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QLinearGradient>
#include <QGraphicsDropShadowEffect>
#include <QSpacerItem>

MainMenu::MainMenu(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("Scavenger Hunt");
    resize(550, 620);
    setMinimumSize(450, 500);

    // --- Layout ---
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 60, 40, 35);
    layout->setSpacing(0);

    // Helper: create a centered, styled label
    auto addLabel = [&](const QString &text, const QString &style) -> QLabel* {
        QLabel *lbl = new QLabel(text, this);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(style);
        layout->addWidget(lbl);
        return lbl;
    };

    // --- Subtitle ---
    addLabel("EMBARK ON YOUR ADVENTURE",
             "color: rgba(200,180,120,0.7); font-size: 11px; font-weight: 600;"
             "letter-spacing: 4px; padding-bottom: 8px;");

    // --- Title with glow ---
    QLabel *title = addLabel("Scavenger\nHunt",
                             "color: #E8D5A3; font-size: 52px; font-weight: 800;"
                             "letter-spacing: 2px; margin: 0px;");
    auto *shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(30);
    shadow->setColor(QColor(200, 160, 60, 100));
    shadow->setOffset(0, 4);
    title->setGraphicsEffect(shadow);

    // --- Divider + description ---
    addLabel("--- + ---",
             "color: rgba(200,170,80,0.4); font-size: 16px;"
             "padding: 10px 0 5px 0; letter-spacing: 3px;");
    addLabel("Choose your difficulty and begin\nthe quest for hidden treasures!",
             "color: rgba(200,200,210,0.65); font-size: 14px;"
             "padding: 5px 20px 15px 20px;");

    layout->addSpacerItem(new QSpacerItem(0, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // --- Difficulty buttons ---
    addLabel("SELECT DIFFICULTY",
             "color: rgba(180,170,140,0.5); font-size: 10px; font-weight: 700;"
             "letter-spacing: 5px; padding-bottom: 12px;");

    QVBoxLayout *btnLayout = new QVBoxLayout;
    btnLayout->setSpacing(12);

    QPushButton *easy   = createButton("EASY",   QColor(80, 200, 120));
    QPushButton *medium = createButton("MEDIUM", QColor(230, 180,  60));
    QPushButton *hard   = createButton("HARD",   QColor(220,  80,  70));

    btnLayout->addWidget(easy,   0, Qt::AlignCenter);
    btnLayout->addWidget(medium, 0, Qt::AlignCenter);
    btnLayout->addWidget(hard,   0, Qt::AlignCenter);
    layout->addLayout(btnLayout);

    // Wire buttons to signal
    connect(easy,   &QPushButton::clicked, this, [this]{ emit difficultySelected(0); });
    connect(medium, &QPushButton::clicked, this, [this]{ emit difficultySelected(1); });
    connect(hard,   &QPushButton::clicked, this, [this]{ emit difficultySelected(2); });

    layout->addSpacerItem(new QSpacerItem(0, 25, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // --- Footer ---
    addLabel("Find the clues. Solve the puzzles. Claim the treasure.",
             "color: rgba(180,170,140,0.35); font-size: 11px; font-style: italic;"
             "padding-top: 8px;");
}

// --- Gradient background (no animations) ---
void MainMenu::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QLinearGradient grad(0, 0, width(), height());
    grad.setColorAt(0.0, QColor(30,  10, 80));
    grad.setColorAt(0.3, QColor(60,  20, 110));
    grad.setColorAt(0.5, QColor(20,  60, 100));
    grad.setColorAt(0.7, QColor(10,  80,  90));
    grad.setColorAt(1.0, QColor(15,  50,  70));
    p.fillRect(rect(), grad);
}

// --- Styled button with accent color ---
QPushButton* MainMenu::createButton(const QString &text, const QColor &c)
{
    QPushButton *btn = new QPushButton(text, this);
    btn->setMinimumSize(240, 52);
    btn->setCursor(Qt::PointingHandCursor);

    auto rgba = [&](double a) {
        return QString("rgba(%1,%2,%3,%4)")
            .arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
    };

    btn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #E8DCC8; border: 1.5px solid %2;"
        "  border-radius: 12px; font: 600 16px; letter-spacing: 1px; }"
        "QPushButton:hover { background: %3; border-color: %4; color: #FFF8E8; }"
        "QPushButton:pressed { background: %5; padding: 2px 0 0 0; }"
    ).arg(rgba(0.40), rgba(0.65), rgba(0.55), rgba(0.90), rgba(0.70)));

    return btn;
}
