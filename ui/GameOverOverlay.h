#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class GameOverOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit GameOverOverlay(QWidget *parent = nullptr);

    void setResult(bool won, int score, bool hasNextLevel);

signals:
    void nextLevelRequested();
    void restartRequested();
    void mainMenuRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel* m_titleLabel;
    QLabel* m_scoreLabel;
    QPushButton* m_nextLevelButton;
    QPushButton* m_restartButton;
    QPushButton* m_mainMenuButton;
};

