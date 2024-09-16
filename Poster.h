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
#include <utility>


// スレッド情報
struct THREAD_INFO
{
    // デフォルトコンストラクタ
    THREAD_INFO() = default;

    // 既存のコンストラクタ
    [[maybe_unused]] THREAD_INFO(QString _subject, QString _from, QString _mail, QString _message,
                                 QString _bbs, QString _time, QString _key) :
                                 subject(std::move(_subject)), from(std::move(_from)), mail(std::move(_mail)), bbs(std::move(_bbs)),
                                 message(std::move(_message)), time(std::move(_time)), key(std::move(_key)) {}

    THREAD_INFO(const QString &_subject, QString &_from, const QString &_mail, const QString &_message,
                const QString &_bbs, const QString &_time, const QString &_key, bool &_shiftjis, const QString &_xpath)
                : subject(_subject), from(_from), mail(_mail), message(_message),
                  bbs(_bbs), time(_time), key(_key), shiftjis(_shiftjis), expiredXPath(_xpath)
    {}

    QString subject      = "";      // スレッドのタイトル (スレッドを新規作成する場合のみ入力)
    QString from         = "";      // 名前欄
    QString mail         = "";      // メール欄
    QString message      = "";      // 書き込む内容
    QString bbs          = "";      // BBS名
    QString time         = "";      // エポックタイム (UNIXタイムまたはPOSIXタイムとも呼ばれる)
    QString key          = "";      // スレッド番号 (スレッドに書き込む場合のみ入力)
    bool    shiftjis     = true;    // Shift-JSIエンコーディングの有効 / 無効
    QString expiredXPath = "";      // スレッドのタイトルを抽出するXPath
};


class Poster : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QNetworkAccessManager> m_pManager;          // 掲示板にアクセスするためのネットワークオブジェクト
    QList<QNetworkCookie>                  m_Cookies;           // クッキーを保存
    QUrl                                   m_URL;               // 書き込み用URL
    QString                                m_NewThreadURL,      // 新規作成したスレッドのURL
                                           m_NewThreadNum,      // 新規作成したスレッド番号
                                           m_NewThreadTitle;    // 新規作成したスレッドタイトル

private:
    // GETデータ(Webページ)を確認する
    int         replyCookieFinished(QNetworkReply *reply);                              // GETデータ(クッキー)を確認する
    int         replyPostFinished(QNetworkReply *reply, THREAD_INFO &ThreadInfo);       // POSTデータ送信後のレスポンスを確認する (既存のスレッドに書き込み用)
    int         replyPostFinished(QNetworkReply *reply, const QUrl &url,                // POSTデータ送信後のレスポンスを確認する (新規スレッド作成用)
                                  const THREAD_INFO &ThreadInfo);
    [[maybe_unused]] static QByteArray  encodeStringToShiftJIS(const QString &str);     // 文字列をShift-JISにエンコードする
    static QString     urlEncode(const QString &originalString);                        // URLエンコードする

public:
    explicit    Poster(QObject *parent = nullptr);
    ~Poster() override = default;
    int         fetchCookies(const QUrl &url);                                  // 掲示板のクッキーを取得する
    int         PostforWriteThread(const QUrl &url, THREAD_INFO &ThreadInfo);   // 特定のスレッドに書き込む
    int         PostforCreateThread(const QUrl &url, THREAD_INFO &ThreadInfo);  // 新規スレッドを作成する
    [[nodiscard]] QString     GetNewThreadURL() const;                          // 新規作成したスレッドのURLを取得する
    [[nodiscard]] QString     GetNewThreadNum() const;                          // 新規作成したスレッド番号を取得する
    [[nodiscard]] QString     GetNewThreadTitle() const;                        // 新規作成したスレッドタイトルを取得する

signals:

};

#endif // POSTER_H
