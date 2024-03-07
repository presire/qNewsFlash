#include <QTimeZone>
#include <iostream>
#include "NtpTimeFetcher.h"


NtpTimeFetcher::NtpTimeFetcher(QObject *parent) : m_pSocket(std::make_unique<QUdpSocket>(this)), QObject{parent}
{
    // タイマの設定
    //connect(&m_timeoutTimer, &QTimer::timeout, this, &NtpTimeFetcher::onTimeout);
    //m_timeoutTimer.setSingleShot(true); // タイマを1度に設定

    connect(m_pSocket.get(), &QUdpSocket::connected, this, &NtpTimeFetcher::onConnected);
    connect(m_pSocket.get(), QOverload<QAbstractSocket::SocketError>::of(&QUdpSocket::errorOccurred), this, &NtpTimeFetcher::onError);
    connect(m_pSocket.get(), &QUdpSocket::readyRead, this, &NtpTimeFetcher::readNtpReply);
}


void NtpTimeFetcher::connectToHostWithTimeout(const QString &host, int timeoutMs)
{
    m_pSocket->abort();                 // 既存の接続を中断
    m_pSocket->connectToHost(host, 123, QIODevice::ReadWrite);
    m_timeoutTimer.start(timeoutMs);    // タイムアウトタイマを開始
}


void NtpTimeFetcher::fetchTime()
{
    // NTPサーバへのリクエストを構成
    QByteArray requestData(48, 0);
    requestData[0] = 0b00100011;  // LI, Version, Mode

    // NTPサーバアドレス
    // サーバアドレスには、"time.google.com"も存在する
    //connectToHostWithTimeout("ntp.nict.jp", 5 * 1000);
    m_pSocket->connectToHost("time.google.com", 123, QIODevice::ReadWrite);
    //m_pSocket->bind(123, QUdpSocket::ShareAddress);

    m_pSocket->write(requestData);
}


void NtpTimeFetcher::readNtpReply()
{
    if (m_pSocket->pendingDatagramSize() > 0) {
        QByteArray replyData = {};
        replyData.resize(m_pSocket->pendingDatagramSize());
        m_pSocket->readDatagram(replyData.data(), replyData.size());

        // NTP応答から時刻を解析
        // 応答から時刻を抽出して変換する必要がある

        // UTC時刻をQDateTimeオブジェクトに変換 (現在時刻を使用)
        QDateTime utcDateTime = QDateTime::currentDateTimeUtc();

        // 日本時間に変換
        m_japanDateTime = utcDateTime.toTimeZone(QTimeZone("Asia/Tokyo"));
    }

    emit finished();
}


QDateTime NtpTimeFetcher::getDateTime() const
{
    return m_japanDateTime;
}


void NtpTimeFetcher::onTimeout()
{
    m_pSocket->abort(); // タイムアウト時に接続試行を中断
}


void NtpTimeFetcher::onConnected()
{
    std::cerr << "接続に成功" << std::endl;
}


void NtpTimeFetcher::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    std::cerr << "接続に失敗 : " << m_pSocket.get()->errorString().toStdString() << std::endl;
}
