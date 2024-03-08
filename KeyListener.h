#ifndef KEYLISTENER_H
#define KEYLISTENER_H

#include <QCoreApplication>
#include <QThread>
#include <iostream>


class KeyListener : public QThread
{
    Q_OBJECT

private:
    std::atomic<bool> m_stopRequested;

public:
    KeyListener();
    ~KeyListener() override;

    void run() override;
    void stop();
};

#endif // KEYLISTENER_H
