#ifndef KEYLISTENER_H
#define KEYLISTENER_H

#include <QCoreApplication>
#include <QThread>
#include <iostream>


class KeyListener : public QThread
{
public:
    KeyListener();
    virtual ~KeyListener();

    void run() override;
};

#endif // KEYLISTENER_H
