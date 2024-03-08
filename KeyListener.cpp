#include "KeyListener.h"

KeyListener::KeyListener() : m_stopRequested(false)
{

}


KeyListener::~KeyListener() = default;


void KeyListener::run()
{
    char key = {};

    do {
        key = std::getchar();
    } while (key != 'Q' && key != 'q' && !m_stopRequested.load());

    QCoreApplication::exit();
}


void KeyListener::stop()
{
    m_stopRequested.store(true);
}
