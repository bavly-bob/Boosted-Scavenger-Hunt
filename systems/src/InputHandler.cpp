#include "InputHandler.h"

#include <Qt>

Direction InputHandler::keyToDirection(int qtKey)
{
    switch (qtKey) {
    case Qt::Key_W:
    case Qt::Key_Up:
        return Direction::Up;
    case Qt::Key_S:
    case Qt::Key_Down:
        return Direction::Down;
    case Qt::Key_A:
    case Qt::Key_Left:
        return Direction::Left;
    case Qt::Key_D:
    case Qt::Key_Right:
        return Direction::Right;
    default:
        return Direction::None;
    }
}

bool InputHandler::isPauseKey(int qtKey)
{
    return qtKey == Qt::Key_Escape;
}
