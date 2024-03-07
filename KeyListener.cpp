#include "KeyListener.h"

KeyListener::KeyListener()
{

}


KeyListener::~KeyListener()
{

}


void KeyListener::run()
{
    char key = {};

    do {
        key = std::getchar();
    } while (key != 'Q' && key != 'q');

    QCoreApplication::exit();
}
