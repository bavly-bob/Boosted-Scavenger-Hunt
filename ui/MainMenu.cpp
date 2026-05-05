#include "MainMenu.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QLinearGradient>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QSpacerItem>
#include <QStackedWidget>

MainMenu::MainMenu(QWidget *parent)
    : QWidget(parent), m_continueBtn(nullptr), m_stackedWidget(nullptr)
{
    setWindowTitle("Scavenger Hunt");
    resize(550, 720);
    setMinimumSize(450, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(m_stackedWidget);

    QWidget *mainMenuPage = new QWidget(m_stackedWidget);
    QVBoxLayout *layout = new QVBoxLayout(mainMenuPage);
    layout->setContentsMargins(40, 60, 40, 35);
    layout->setSpacing(0);

    auto addLabel = [&](const QString &text, const QString &style, QVBoxLayout *targetLayout) -> QLabel* {
        QLabel *lbl = new QLabel(text, this);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(style);
        targetLayout->addWidget(lbl);
        return lbl;
    };

    addLabel("EMBARK ON YOUR ADVENTURE",
             "color: rgba(200,180,120,0.7); font-size: 11px; font-weight: 600;"
             "letter-spacing: 4px; padding-bottom: 8px;", layout);

    QLabel *title = addLabel("Scavenger\nHunt",
                             "color: #E8D5A3; font-size: 52px; font-weight: 800;"
                             "letter-spacing: 2px; margin: 0px;", layout);
    auto *shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(30);
    shadow->setColor(QColor(200, 160, 60, 100));
    shadow->setOffset(0, 4);
    title->setGraphicsEffect(shadow);

    addLabel("--- + ---",
             "color: rgba(200,170,80,0.4); font-size: 16px;"
             "padding: 10px 0 5px 0; letter-spacing: 3px;", layout);
    addLabel("Choose your difficulty and begin\nthe quest for hidden treasures!",
             "color: rgba(200,200,210,0.65); font-size: 14px;"
             "padding: 5px 20px 15px 20px;", layout);

    layout->addSpacerItem(new QSpacerItem(0, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

    addLabel("SELECT DIFFICULTY",
             "color: rgba(180,170,140,0.5); font-size: 10px; font-weight: 700;"
             "letter-spacing: 5px; padding-bottom: 12px;", layout);

    QVBoxLayout *btnLayout = new QVBoxLayout;
    btnLayout->setSpacing(36);

    QPushButton *easy   = createButton("EASY",   QColor(80, 200, 120));
    QPushButton *medium = createButton("MEDIUM", QColor(230, 180,  60));
    QPushButton *hard   = createButton("HARD",   QColor(220,  80,  70));
    QPushButton *multiplayer = createButton("MULTIPLAYER", QColor(150, 100, 200));
    
    m_continueBtn = createButton("CONTINUE", QColor(60, 120, 200));
    btnLayout->addWidget(m_continueBtn, 0, Qt::AlignCenter);
    connect(m_continueBtn, &QPushButton::clicked, this, [this]{ emit continueRequested(); });

    QPushButton *quit   = createButton("QUIT GAME", QColor(100, 100, 100));

    btnLayout->addWidget(easy,   0, Qt::AlignCenter);
    btnLayout->addWidget(medium, 0, Qt::AlignCenter);
    btnLayout->addWidget(hard,   0, Qt::AlignCenter);
    btnLayout->addWidget(multiplayer, 0, Qt::AlignCenter);
    btnLayout->addWidget(quit,   0, Qt::AlignCenter);
    layout->addLayout(btnLayout);

    connect(easy,   &QPushButton::clicked, this, [this]{ emit difficultySelected(0); });
    connect(medium, &QPushButton::clicked, this, [this]{ emit difficultySelected(1); });
    connect(hard,   &QPushButton::clicked, this, [this]{ emit difficultySelected(2); });
    connect(multiplayer, &QPushButton::clicked, this, [this]{ m_stackedWidget->setCurrentIndex(1); });
    connect(quit,   &QPushButton::clicked, this, [this]{ emit quitRequested(); });

    layout->addSpacerItem(new QSpacerItem(0, 25, QSizePolicy::Minimum, QSizePolicy::Expanding));

    addLabel("Find the clues. Solve the puzzles. Claim the treasure.",
             "color: rgba(180,170,140,0.35); font-size: 11px; font-style: italic;"
             "padding-top: 8px;", layout);

    m_stackedWidget->addWidget(mainMenuPage);

    QWidget *multiplayerPage = new QWidget(m_stackedWidget);
    QVBoxLayout *mpLayout = new QVBoxLayout(multiplayerPage);
    mpLayout->setContentsMargins(40, 60, 40, 35);
    mpLayout->setSpacing(0);

    addLabel("MULTIPLAYER",
             "color: rgba(200,180,120,0.7); font-size: 11px; font-weight: 600;"
             "letter-spacing: 4px; padding-bottom: 8px;", mpLayout);

    QLabel *mpTitle = addLabel("Team Up\nOr Compete",
                             "color: #E8D5A3; font-size: 42px; font-weight: 800;"
                             "letter-spacing: 2px; margin: 0px;", mpLayout);
    auto *mpShadow = new QGraphicsDropShadowEffect;
    mpShadow->setBlurRadius(30);
    mpShadow->setColor(QColor(200, 160, 60, 100));
    mpShadow->setOffset(0, 4);
    mpTitle->setGraphicsEffect(mpShadow);

    addLabel("--- + ---",
             "color: rgba(200,170,80,0.4); font-size: 16px;"
             "padding: 10px 0 5px 0; letter-spacing: 3px;", mpLayout);
    addLabel("Join a friend's adventure or\nhost your own game!",
             "color: rgba(200,200,210,0.65); font-size: 14px;"
             "padding: 5px 20px 15px 20px;", mpLayout);

    mpLayout->addSpacerItem(new QSpacerItem(0, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

    QVBoxLayout *mpBtnLayout = new QVBoxLayout;
    mpBtnLayout->setSpacing(36);

    QPushButton *hostBtn = createButton("HOST GAME", QColor(220, 120, 60));
    QPushButton *joinBtn = createButton("JOIN GAME", QColor(60, 180, 220));
    QPushButton *backBtn = createButton("BACK", QColor(100, 100, 100));

    mpBtnLayout->addWidget(hostBtn, 0, Qt::AlignCenter);
    mpBtnLayout->addWidget(joinBtn, 0, Qt::AlignCenter);
    mpBtnLayout->addWidget(backBtn, 0, Qt::AlignCenter);
    mpLayout->addLayout(mpBtnLayout);

    connect(backBtn, &QPushButton::clicked, this, [this]{
        m_stackedWidget->setCurrentIndex(0);
    });
    connect(hostBtn, &QPushButton::clicked, this, [this]{
        emit multiplayerRequested(true);
    });
    connect(joinBtn, &QPushButton::clicked, this, [this]{
        emit multiplayerRequested(false);
    });

    mpLayout->addSpacerItem(new QSpacerItem(0, 25, QSizePolicy::Minimum, QSizePolicy::Expanding));

    m_stackedWidget->addWidget(multiplayerPage);
}

void MainMenu::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);

    QLinearGradient grad(0, 0, 0, height());
    grad.setColorAt(0.0, QColor(15, 20, 30));
    grad.setColorAt(1.0, QColor(5, 5, 10));

    p.fillRect(rect(), grad);
}

void MainMenu::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_continueBtn) {
        m_continueBtn->setVisible(QFile::exists("save.json"));
    }
}

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
