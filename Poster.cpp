#include <QTextCodec>
#include <iostream>
#include "Poster.h"


Poster::Poster(QObject *parent) : m_pManager(std::make_unique<QNetworkAccessManager>(this)), QObject{parent}
{

}


int Poster::fetchCookies(const QUrl &url)
{
    // レスポンス待機の設定
    QEventLoop loop;
    connect(m_pManager.get(), &QNetworkAccessManager::finished, &loop, &QEventLoop::quit);

    // クッキーの取得
    QNetworkRequest request(url);
    auto pReply = m_pManager->get(request);

    // レスポンス待機
    loop.exec();

    return replyCookieFinished(pReply);
}


int Poster::replyCookieFinished(QNetworkReply *reply)
{
    // レスポンスからクッキーを取得
    QList<QNetworkCookie> receivedCookies = QNetworkCookie::parseCookies(reply->rawHeader("Set-Cookie"));
    if (!receivedCookies.isEmpty()) {
        m_Cookies = receivedCookies;  // クッキーを保存
    }
    else {
        std::cerr << QString("エラー : クッキーの取得に失敗").toStdString() << std::endl;

        return -1;
    }

    reply->deleteLater();

    return 0;
}


int Poster::Post(const QUrl &url, THREAD_INFO &threadInfo)
{
    // リクエストの作成
    QNetworkRequest request(url);

    // POSTデータの生成 (<form>タグの<input>要素に基づいてデータを設定)
    QTextCodec *codec          = nullptr;   // Shift-JIS用
    QByteArray encodedPostData = {};

    if (!threadInfo.shiftjis) {
        // UTF-8用 (QUrlQueryクラスはShift-JISに非対応)
        QUrlQuery query;

        query.addQueryItem("subject",   threadInfo.subject);
        query.addQueryItem("FROM",      threadInfo.from);
        query.addQueryItem("mail",      threadInfo.mail);       // メール欄
        query.addQueryItem("MESSAGE",   threadInfo.message);    // 書き込む内容
        query.addQueryItem("bbs",       threadInfo.bbs);        // BBS名
        query.addQueryItem("time",      threadInfo.time);       // 時刻
        query.addQueryItem("key",       threadInfo.key);        // 書き込むスレッド番号

        encodedPostData = query.toString(QUrl::FullyEncoded).toUtf8();  // POSTデータの変換
    }
    else {
        // Shift-JIS用
        QString postMessage = QString("subject=%1&FROM=%2&mail=%3&MESSAGE=%4&bbs=%5&time=%6&key=%7")
                                  .arg(threadInfo.subject,  // スレッドのタイトル (スレッドを立てる場合のみ入力)
                                       threadInfo.from,
                                       threadInfo.mail,     // メール欄
                                       threadInfo.message,  // 書き込む内容
                                       threadInfo.bbs,      // BBS名
                                       threadInfo.time,     // 時刻
                                       threadInfo.key       // 書き込むスレッド番号
                                       );
        auto postData   = postMessage.toUtf8();             // POSTデータの変換

        // POSTデータの文字コードをShift-JISへエンコード
        codec           = QTextCodec::codecForName("Shift-JIS");
        encodedPostData = codec->fromUnicode(postData);
    }

    // ContentTypeHeaderをHTTPリクエストに設定
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QString::number(encodedPostData.length()));

    // クッキーをHTTPリクエストに設定
    QVariant var;
    var.setValue(m_Cookies);
    request.setHeader(QNetworkRequest::CookieHeader, var);

    // レスポンス待機の設定
    QEventLoop loop;
    connect(m_pManager.get(), &QNetworkAccessManager::finished, &loop, &QEventLoop::quit);

    // HTTPリクエストの送信
    auto pReply = m_pManager->post(request, encodedPostData);

    // レスポンス待機
    loop.exec();

    // レスポンス情報の取得
    return replyPostFinished(pReply);
}


int Poster::replyPostFinished(QNetworkReply *reply)
{
    if (reply->error()) {
        std::cerr << QString("書き込みエラー : %1").arg(reply->errorString()).toStdString() << std::endl;
        return -1;
    }
    else {
#ifdef _DEBUG
        qDebug() << reply->readAll();
#endif
    }

    reply->deleteLater();

    return 0;
}


// 文字列をShift-JISにエンコードする関数
QByteArray Poster::encodeStringToShiftJIS(const QString &str)
{
    QTextCodec *codec = QTextCodec::codecForName("Shift-JIS");
    return codec->fromUnicode(str);
}


// エンコードされたバイト列をURLエンコードする関数
QString Poster::urlEncode(QByteArray byteArray)
{
    return QString::fromUtf8(byteArray.toPercentEncoding());
}
