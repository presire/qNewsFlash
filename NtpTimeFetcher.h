#ifndef NTPTIMEFETCHER_H
#define NTPTIMEFETCHER_H

#include <QObject>
#include <QUdpSocket>
#include <QDateTime>
#include <QTimer>
#include <memory>


class NtpTimeFetcher : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QUdpSocket> m_pSocket;
    QTimer                      m_timeoutTimer;
    QDateTime                   m_japanDateTime;

private:
    void connectToHostWithTimeout(const QString &host, int timeoutMs);

public:
    explicit NtpTimeFetcher(QObject *parent = nullptr);
    void fetchTime();
    QDateTime getDateTime() const;

signals:
    void finished();

private slots:
    void onTimeout();
    void onConnected();
    void onError(QAbstractSocket::SocketError socketError);
    void readNtpReply();
};

#endif // NTPTIMEFETCHER_H
