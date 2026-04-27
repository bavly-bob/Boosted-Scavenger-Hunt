#ifndef MAINMENU_H
#define MAINMENU_H

#include <QWidget>
#include <QPushButton>

class QStackedWidget;

class MainMenu : public QWidget
{
    Q_OBJECT

public:
    explicit MainMenu(QWidget *parent = nullptr);

signals:
    void difficultySelected(int difficulty);
    void continueRequested();
    void multiplayerRequested();
    void quitRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QPushButton* createButton(const QString &text, const QColor &color);
    
    QPushButton* m_continueBtn;
    QStackedWidget* m_stackedWidget;
};

#endif // MAINMENU_H
