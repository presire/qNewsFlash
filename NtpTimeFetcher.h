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
    QString                     m_ntpServer;

private:
    quint64 parseNtpTimestamp(const QByteArray &data, int offset);

public:
    explicit NtpTimeFetcher(QObject *parent = nullptr);
    void fetchTime(const QString &server = "ntp.nict.jp");
    QDateTime getDateTime() const;

signals:
    void finished();
    void error(const QString &errorMessage);

private slots:
    void onTimeout();
    void onError(QAbstractSocket::SocketError socketError);
    void readNtpReply();
};

#endif // NTPTIMEFETCHER_H
