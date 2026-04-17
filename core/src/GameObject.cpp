#include "GameObject.h"

GameObject::GameObject(int x, int y)
    : m_x(x),
      m_y(y)
{
}

int GameObject::getX() const
{
    return m_x;
}

int GameObject::getY() const
{
    return m_y;
}

void GameObject::setPosition(int x, int y)
{
    m_x = x;
    m_y = y;
}
