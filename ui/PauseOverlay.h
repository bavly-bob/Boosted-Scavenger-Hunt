#pragma once

#include <QWidget>

class QPushButton;

class PauseOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit PauseOverlay(QWidget *parent = nullptr);

signals:
    void resumeRequested();
    void restartRequested();
    void mainMenuRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPushButton* m_resumeButton;
    QPushButton* m_restartButton;
    QPushButton* m_mainMenuButton;
};

