#include "ui/MainMenu.h"
#include "ui/GameWindow.h"
#include "Enums.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainMenu menu;
    GameWindow *gameWindow = nullptr;

    QObject::connect(&menu, &MainMenu::difficultySelected, [&](int difficulty){
        menu.hide();
        if (!gameWindow) {
            gameWindow = new GameWindow();

            QObject::connect(gameWindow, &GameWindow::quitToMainMenuRequested, &menu, [&]() {
                gameWindow->hide();
                menu.show();
                menu.raise();
                menu.activateWindow();
            });
        }
        Difficulty diff = Difficulty::NORMAL;
        switch (difficulty) {
        case 0:
            diff = Difficulty::EASY;
            break;
        case 1:
            diff = Difficulty::NORMAL;
            break;
        case 2:
            diff = Difficulty::HARD;
            break;
        default:
            break;
        }

        gameWindow->startNewGame(diff);
    });

    menu.show();
    return app.exec();
}
