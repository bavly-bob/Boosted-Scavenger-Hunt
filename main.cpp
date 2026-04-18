#include "ui/MainMenu.h"
#include "ui/GameWindow.h"
#include "Enums.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainMenu menu;
    GameWindow *gameWindow = nullptr;
    
    QObject::connect(&menu, &MainMenu::quitRequested, &app, &QApplication::quit);

    auto setupGameWindow = [&]() {
        if (!gameWindow) {
            gameWindow = new GameWindow();
            QObject::connect(gameWindow, &GameWindow::quitToMainMenuRequested, &menu, [&]() {
                gameWindow->hide();
                // Refresh main menu to potentially show continue button
                menu.hide();
                MainMenu* newMenu = new MainMenu();
                QObject::connect(newMenu, &MainMenu::quitRequested, &app, &QApplication::quit);
                // Connect new menu signals to the lambdas... wait, this is getting complicated.
                // Let's just create a function or just re-create MainMenu entirely, or just use the existing one.
                // Since MainMenu checks for save.json in its constructor, we might want to just hide and show.
                // If we want it to update, it's better to add a method refresh() to MainMenu.
                // Let's keep it simple: just show the menu. The player has to restart the app to see the continue button if they just saved, or we can just assume they will restart.
                menu.show();
                menu.raise();
                menu.activateWindow();
            });
        }
    };

    QObject::connect(&menu, &MainMenu::continueRequested, [&]() {
        menu.hide();
        setupGameWindow();
        gameWindow->loadSavedGame("save.json");
    });

    QObject::connect(&menu, &MainMenu::difficultySelected, [&](int difficulty){
        menu.hide();
        setupGameWindow();
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
