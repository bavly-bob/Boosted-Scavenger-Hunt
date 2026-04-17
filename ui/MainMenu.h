#ifndef MAINMENU_H
#define MAINMENU_H

#include <QWidget>
#include <QPushButton>

// MainMenu — first screen the player sees.
// Shows the game title and difficulty buttons over a gradient background.
class MainMenu : public QWidget
{
    Q_OBJECT

public:
    explicit MainMenu(QWidget *parent = nullptr);

signals:
    // Emitted when a difficulty button is clicked (0=Easy, 1=Medium, 2=Hard)
    void difficultySelected(int difficulty);

protected:
    // Paints the gradient background
    void paintEvent(QPaintEvent *event) override;

private:
    // Helper: creates a styled button with the given accent color
    QPushButton* createButton(const QString &text, const QColor &color);
};

#endif // MAINMENU_H
