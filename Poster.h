#ifndef POSTER_H
#define POSTER_H

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include <QUrl>
#include <QUrlQuery>
#include <QObject>
#include <memory>


// 書き込むスレッド
struct THREAD_INFO
{
    THREAD_INFO() {};
    THREAD_INFO(QString _subject, QString _from, QString _mail, QString _message, QString _bbs, QString _time, QString _key)
        : subject(_subject), from(_from), mail(_mail), bbs(_bbs), time(_time), key(_key) {
    }
    QString subject     = "";
    QString from        = "";
    QString mail        = "";
    QString message     = "";
    QString bbs         = "";
    QString time        = "";
    QString key         = "";
    bool    shiftjis    = true;
};


class Poster : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QNetworkAccessManager> m_pManager;
    QList<QNetworkCookie>                  m_Cookies;   // クッキーを保存
    QUrl                                   m_URL;       // 書き込み用URL

private:
    int         replyCookieFinished(QNetworkReply *reply);
    int         replyPostFinished(QNetworkReply *reply);
    QByteArray  encodeStringToShiftJIS(const QString &str);
    QString     urlEncode(QByteArray byteArray);

public:
    explicit    Poster(QObject *parent = nullptr);
    ~Poster() override = default;
    int         fetchCookies(const QUrl &url);
    int         Post(const QUrl &url, THREAD_INFO &threadInfo);

signals:

};

#endif // POSTER_H
