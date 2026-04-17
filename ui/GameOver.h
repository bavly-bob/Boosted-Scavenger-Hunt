#ifndef GAMEOVER_H
#define GAMEOVER_H

#include <QWidget>

namespace Ui {
class GameOver;
}

class GameOver : public QWidget
{
    Q_OBJECT

public:
    explicit GameOver(QWidget *parent = nullptr);
    ~GameOver();

    // Preliminary functions
    void setScore(int score);
    void setGameWon(bool won);

signals:
    void returnToMainMenu();
    void playAgain();

private:
    Ui::GameOver *ui;
};

#endif // GAMEOVER_H
