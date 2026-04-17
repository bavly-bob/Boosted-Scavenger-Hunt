#ifndef GAMESCREEN_H
#define GAMESCREEN_H

#include <QWidget>

namespace Ui {
class GameScreen;
}

class GameScreen : public QWidget
{
    Q_OBJECT

public:
    explicit GameScreen(QWidget *parent = nullptr);
    ~GameScreen();

    // Preliminary functions
    void startGame();
    void pauseGame();
    void resumeGame();

signals:
    void gameOverSignal(bool won, int score);

private:
    Ui::GameScreen *ui;
};

#endif // GAMESCREEN_H
