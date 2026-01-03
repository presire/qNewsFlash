#include <QTimeZone>
#include <QHostInfo>
#include <iostream>
#include "NtpTimeFetcher.h"


NtpTimeFetcher::NtpTimeFetcher(QObject *parent) 
    : QObject{parent}
    , m_pSocket(std::make_unique<QUdpSocket>(this))
{
    // タイマの設定
    connect(&m_timeoutTimer, &QTimer::timeout, this, &NtpTimeFetcher::onTimeout);
    m_timeoutTimer.setSingleShot(true);

    // UDPソケットのシグナル接続
    connect(m_pSocket.get(), QOverload<QAbstractSocket::SocketError>::of(&QUdpSocket::errorOccurred), 
            this, &NtpTimeFetcher::onError);
    connect(m_pSocket.get(), &QUdpSocket::readyRead, this, &NtpTimeFetcher::readNtpReply);
}


void NtpTimeFetcher::fetchTime(const QString &server)
{
    m_ntpServer = server;
    
    // ソケットを初期化
    m_pSocket->abort();
    
    // NTPリクエストパケットを構成（48バイト）
    QByteArray requestData(48, 0);
    requestData[0] = 0b00100011;  // LI=0, Version=4, Mode=3 (client)
    
    std::cerr << "NTPサーバーに接続中: " << server.toStdString() << std::endl;
    
    // UDPなのでwriteDatagramを使用（これが重要！）
    qint64 bytesWritten = m_pSocket->writeDatagram(requestData, QHostAddress(server), 123);
    
    if (bytesWritten == -1) {
        // ホスト名の場合は名前解決が必要
        QHostInfo::lookupHost(server, this, [this, requestData](const QHostInfo &host) {
            if (host.error() != QHostInfo::NoError) {
                std::cerr << "ホスト名の解決に失敗: " << host.errorString().toStdString() << std::endl;
                emit error("ホスト名の解決に失敗: " + host.errorString());
                emit finished();
                return;
            }
            
            if (host.addresses().isEmpty()) {
                std::cerr << "ホストのアドレスが見つかりません" << std::endl;
                emit error("ホストのアドレスが見つかりません");
                emit finished();
                return;
            }
            
            QHostAddress ntpAddress = host.addresses().first();
            std::cerr << "解決されたアドレス: " << ntpAddress.toString().toStdString() << std::endl;
            
            qint64 bytes = m_pSocket->writeDatagram(requestData, ntpAddress, 123);
            if (bytes == -1) {
                std::cerr << "データ送信に失敗" << std::endl;
                emit error("データ送信に失敗");
                emit finished();
            } else {
                std::cerr << "NTPリクエスト送信: " << bytes << " バイト" << std::endl;
            }
        });
    } else {
        std::cerr << "NTPリクエスト送信: " << bytesWritten << " バイト" << std::endl;
    }
    
    // タイムアウトタイマを開始（5秒）
    m_timeoutTimer.start(5000);
}


void NtpTimeFetcher::readNtpReply()
{
    m_timeoutTimer.stop();  // タイムアウトタイマを停止
    
    while (m_pSocket->hasPendingDatagrams()) {
        QByteArray replyData;
        replyData.resize(m_pSocket->pendingDatagramSize());
        
        QHostAddress sender;
        quint16 senderPort;
        m_pSocket->readDatagram(replyData.data(), replyData.size(), &sender, &senderPort);
        
        std::cerr << "NTP応答受信: " << replyData.size() << " バイト from " 
                  << sender.toString().toStdString() << std::endl;
        
        if (replyData.size() < 48) {
            std::cerr << "NTP応答が短すぎます" << std::endl;
            emit error("NTP応答が短すぎます");
            emit finished();
            return;
        }
        
        // NTPタイムスタンプを解析（バイト40-43が送信タイムスタンプの秒部分）
        // NTPタイムスタンプは1900年1月1日からの秒数
        quint64 ntpTimestamp = parseNtpTimestamp(replyData, 40);
        
        // NTPタイムスタンプをUnixタイムスタンプに変換
        // 1900年1月1日から1970年1月1日までの秒数を引く
        const quint64 NTP_UNIX_OFFSET = 2208988800ULL;
        quint64 unixTimestamp = ntpTimestamp - NTP_UNIX_OFFSET;
        
        // QDateTimeに変換
        QDateTime utcDateTime = QDateTime::fromSecsSinceEpoch(unixTimestamp, Qt::UTC);
        
        // 日本時間に変換
        m_japanDateTime = utcDateTime.toTimeZone(QTimeZone("Asia/Tokyo"));
        
        std::cerr << "取得した時刻: " << m_japanDateTime.toString(Qt::ISODate).toStdString() << std::endl;
    }
    
    emit finished();
}


quint64 NtpTimeFetcher::parseNtpTimestamp(const QByteArray &data, int offset)
{
    // NTPタイムスタンプは64ビット（最初の32ビットが秒、次の32ビットが秒の小数部分）
    // ここでは秒部分のみを取得
    quint64 seconds = 0;
    for (int i = 0; i < 4; ++i) {
        seconds = (seconds << 8) | static_cast<quint8>(data[offset + i]);
    }
    return seconds;
}


QDateTime NtpTimeFetcher::getDateTime() const
{
    return m_japanDateTime;
}


void NtpTimeFetcher::onTimeout()
{
    std::cerr << "NTP要求がタイムアウトしました" << std::endl;
    m_pSocket->abort();
    emit error("NTP要求がタイムアウトしました");
    emit finished();
}


void NtpTimeFetcher::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    std::cerr << "ソケットエラー: " << m_pSocket->errorString().toStdString() << std::endl;
    emit error(m_pSocket->errorString());
}
