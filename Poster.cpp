#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    #include <QStringEncoder>
#else
    #include <QTextCodec>
#endif

#include <iostream>
#include "Poster.h"
#include "HtmlFetcher.h"


Poster::Poster(QObject *parent) : m_pManager(std::make_unique<QNetworkAccessManager>(this)), QObject{parent}
{

}


// 掲示板のクッキーを取得する
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


// GETデータ(クッキー)を確認する
int Poster::replyCookieFinished(QNetworkReply *reply)
{
    // レスポンスからクッキーを取得
    QList<QNetworkCookie> receivedCookies = QNetworkCookie::parseCookies(reply->rawHeader("Set-Cookie"));
    if (!receivedCookies.isEmpty()) {
        // クッキーを保存
        m_Cookies = receivedCookies;
    }
    else {
        // クッキーの取得に失敗した場合
        std::cerr << QString("エラー : クッキーの取得に失敗").toStdString() << std::endl;
        reply->deleteLater();

        return -1;
    }

    reply->deleteLater();

    return 0;
}


// 特定のスレッドに書き込む
int Poster::PostforWriteThread(const QUrl &url, THREAD_INFO &ThreadInfo)
{
    // リクエストの作成
    QNetworkRequest request(url);

    // POSTデータの生成 (<form>タグの<input>要素に基づいてデータを設定)
    // 既存のスレッドに書き込む場合は、<input>要素のname属性の値"key"にスレッド番号を指定する必要がある
    QByteArray encodedPostData = {};    // エンコードされたバイト列

    if (!ThreadInfo.shiftjis) {
        // UTF-8用 (QUrlQueryクラスはShift-JISに非対応)
        QUrlQuery query;

        query.addQueryItem("subject",   "");                            // スレッドのタイトル (スレッドに書き込む場合は空欄にする)
        query.addQueryItem("FROM",      ThreadInfo.from);               // 名前欄
        query.addQueryItem("mail",      ThreadInfo.mail);               // メール欄
        query.addQueryItem("MESSAGE",   urlEncode(ThreadInfo.message)); // 書き込む内容
        query.addQueryItem("bbs",       ThreadInfo.bbs);                // BBS名
        query.addQueryItem("time",      ThreadInfo.time);               // エポックタイム (UNIXタイムまたはPOSIXタイム)
        query.addQueryItem("key",       ThreadInfo.key);                // 書き込むスレッド番号 (スレッドに書き込む場合は入力する)

        encodedPostData = query.toString(QUrl::FullyEncoded).toUtf8();  // POSTデータをバイト列へ変換
    }
    else {
        // Shift-JIS用
        // Shift-JISに変換不可能な文字が存在する場合は、文字参照に変換
        ThreadInfo.from    = convertNonSjisToReference(ThreadInfo.from);
        ThreadInfo.message = convertNonSjisToReference(ThreadInfo.message);

        QString postMessage = QString("subject=%1&FROM=%2&mail=%3&MESSAGE=%4&bbs=%5&time=%6&key=%7")
                                  .arg("",                              // スレッドのタイトル (スレッドに書き込む場合は空欄にする)
                                       urlEncode(ThreadInfo.from),      // 名前欄
                                       urlEncode(ThreadInfo.mail),      // メール欄
                                       urlEncode(ThreadInfo.message),   // 書き込む内容
                                       ThreadInfo.bbs,                  // BBS名
                                       ThreadInfo.time,                 // エポックタイム (UNIXタイムまたはPOSIXタイム)
                                       ThreadInfo.key                   // 書き込むスレッド番号 (スレッドに書き込む場合は入力する)
                                       );
        auto postData   = postMessage.toUtf8();             // POSTデータをバイト列へ変換

        // POSTデータの文字コードをShift-JISへエンコード
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QStringEncoder encoder("Shift-JIS");
        encodedPostData = encoder(postData);
#else
        QTextCodec *codec;  // Shift-JIS用エンコードオブジェクト
        codec           = QTextCodec::codecForName("Shift-JIS");
        encodedPostData = codec->fromUnicode(postData);
#endif
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
    return replyPostFinished(pReply, ThreadInfo);
}


// 新規スレッドを作成する
int Poster::PostforCreateThread(const QUrl &url, THREAD_INFO &ThreadInfo)
{
    // リクエストの作成
    QNetworkRequest request(url);

    // POSTデータの生成 (<form>タグの<input>要素に基づいてデータを設定)
    // 新規スレッドを作成する場合は、<input>要素のname属性の値"key"を除去する必要がある
    QByteArray encodedPostData = {};    // エンコードされたバイト列

    if (!ThreadInfo.shiftjis) {
        // UTF-8用 (QUrlQueryクラスはShift-JISに非対応)
        QUrlQuery query;

        query.addQueryItem("subject",   urlEncode(ThreadInfo.subject)); // スレッドのタイトル (スレッドを新規作成する場合は入力する)
        query.addQueryItem("FROM",      ThreadInfo.from);               // 名前欄
        query.addQueryItem("mail",      ThreadInfo.mail);               // メール欄
        query.addQueryItem("MESSAGE",   urlEncode(ThreadInfo.message)); // 書き込む内容
        query.addQueryItem("bbs",       ThreadInfo.bbs);                // BBS名
        query.addQueryItem("time",      ThreadInfo.time);               // エポックタイム (UNIXタイムまたはPOSIXタイム)

        encodedPostData = query.toString(QUrl::FullyEncoded).toUtf8();  // POSTデータをバイト列へ変換
    }
    else {
        // Shift-JIS用
        // Shift-JISに変換不可能な文字が存在する場合は、文字参照に変換
        ThreadInfo.subject = convertNonSjisToReference(ThreadInfo.subject);
        ThreadInfo.from    = convertNonSjisToReference(ThreadInfo.from);
        ThreadInfo.message = convertNonSjisToReference(ThreadInfo.message);

        QString postMessage = QString("subject=%1&FROM=%2&mail=%3&MESSAGE=%4&bbs=%5&time=%6")
                                  .arg(urlEncode(ThreadInfo.subject),   // スレッドのタイトル (スレッドを立てる場合のみ入力)
                                       urlEncode(ThreadInfo.from),      // 名前欄
                                       urlEncode(ThreadInfo.mail),      // メール欄
                                       urlEncode(ThreadInfo.message),   // 書き込む内容
                                       ThreadInfo.bbs,                  // BBS名
                                       ThreadInfo.time                  // エポックタイム (UNIXタイムまたはPOSIXタイム)
                                       );
        auto postData   = postMessage.toUtf8();             // POSTデータをバイト列へ変換

        // POSTデータの文字コードをShift-JISへエンコード
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QStringEncoder encoder("Shift-JIS");
        encodedPostData = encoder(postData);
#else
        QTextCodec *codec;  // Shift-JIS用エンコードオブジェクト
        codec           = QTextCodec::codecForName("Shift-JIS");
        encodedPostData = codec->fromUnicode(postData);
#endif
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
    return replyPostFinished(pReply, url, ThreadInfo);
}


// POSTデータ送信後のレスポンスを確認する (既存のスレッドに書き込み用)
int Poster::replyPostFinished(QNetworkReply *reply, THREAD_INFO &ThreadInfo)
{
    if (reply->error()) {
        std::cerr << QString("書き込みエラー : %1").arg(reply->errorString()).toStdString() << std::endl;
        reply->deleteLater();

        return -1;
    }
    else {
        QString replyData;

        if (ThreadInfo.shiftjis) {
            // Shift-JISからUTF-8へエンコード
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QStringEncoder encoder("Shift-JIS");
            replyData = encoder(reply->readAll());
#else
            QTextCodec *codec;
            codec     = QTextCodec::codecForName("Shift-JIS");
            replyData = codec->toUnicode(reply->readAll());
#endif
        }
        else {
            replyData = reply->readAll();
        }

#ifdef _DEBUG
        std::cout << replyData.toStdString() << std::endl;
#endif

        // 書き込んだスレッドのパスと番号を取得
        HtmlFetcher fetcher(nullptr);
        if (fetcher.extractThreadPath(replyData, ThreadInfo.bbs)) {
            std::cerr << QString("エラー : スレッドのURLとスレッド番号の取得に失敗").toStdString() << std::endl;
            std::cerr << QString("スレッドの書き込みに失敗した可能性があります").toStdString() << std::endl;
            reply->deleteLater();

            return -1;
        }

        // 書き込みに成功した場合、レスポンス内容にスレッドのパスと番号が含まれている
        // それらの情報が無ければ書き込みは失敗したと考えられる
        if (fetcher.GetThreadPath().isEmpty() || fetcher.GetThreadNum().isEmpty()) {
            std::cerr << QString("エラー : スレッドのURLまたはスレッド番号がありません").toStdString() << std::endl;
            std::cerr << QString("スレッドの書き込みに失敗した可能性があります").toStdString() << std::endl;
            reply->deleteLater();

            return -1;
        }
    }

    reply->deleteLater();

    return 0;
}


// POSTデータ送信後のレスポンスを確認する (新規スレッド作成用)
int Poster::replyPostFinished(QNetworkReply *reply, const QUrl &url, const THREAD_INFO &ThreadInfo)
{
    if (reply->error()) {
        std::cerr << QString("書き込みエラー : %1").arg(reply->errorString()).toStdString() << std::endl;
        reply->deleteLater();

        return -1;
    }
    else {
        QString replyData;

        if (ThreadInfo.shiftjis) {
            // Shift-JISからUTF-8へデコード
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QStringDecoder decoder("Shift-JIS");
            replyData = decoder(reply->readAll());
#else
            QTextCodec *codec;
            codec     = QTextCodec::codecForName("Shift-JIS");
            replyData = codec->toUnicode(reply->readAll());
#endif
        }
        else {
            replyData = reply->readAll();
        }

#ifdef _DEBUG
        std::cout << replyData.toStdString() << std::endl;
#endif

        // 新規作成したスレッドのURLのパスを取得
        HtmlFetcher fetcher(nullptr);
        if (fetcher.extractThreadPath(replyData, ThreadInfo.bbs)) {
            std::cerr << QString("エラー : 新規作成したスレッドのURLとスレッド番号の取得に失敗").toStdString() << std::endl;
            std::cerr << QString("スレッドの新規作成に失敗した可能性があります").toStdString() << std::endl;
            reply->deleteLater();

            return -1;
        }

        if (fetcher.GetThreadPath().isEmpty() || fetcher.GetThreadNum().isEmpty()) {
            std::cerr << QString("エラー : 新規作成したスレッドのURLまたはスレッド番号がありません").toStdString() << std::endl;
            std::cerr << QString("スレッドの新規作成に失敗した可能性があります").toStdString() << std::endl;
            reply->deleteLater();

            return -1;
        }

        // ベースURLを構築
        QUrl baseUrl = url.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment);

        // ベースURLとパスを繋げて新規作成したスレッドのURLを取得
        m_NewThreadURL = baseUrl.scheme() + "://" + baseUrl.host() + fetcher.GetThreadPath();

        // 新規作成したスレッド番号を取得
        m_NewThreadNum = fetcher.GetThreadNum();

        // 新規作成したスレッドにアクセスしてタイトルを抽出
        if (fetcher.extractThreadTitle(QUrl(m_NewThreadURL), true, ThreadInfo.expiredXPath, ThreadInfo.shiftjis) == 0) {
            // 新規作成したスレッドのタイトル抽出に成功した場合
            m_NewThreadTitle = fetcher.GetElement();
        }
        else {
            std::cerr << QString("スレッドタイトルの抽出に失敗しました").toStdString() << std::endl;
            reply->deleteLater();

            return -1;
        }
    }

    reply->deleteLater();

    return 0;
}


// 新規作成したスレッドのパスを取得する
QString Poster::GetNewThreadURL() const
{
    return m_NewThreadURL;
}


// 新規作成したスレッドのスレッド番号を取得する
QString Poster::GetNewThreadNum() const
{
    return m_NewThreadNum;
}


// 新規作成したスレッドのスレッド番号を取得する
QString Poster::GetNewThreadTitle() const
{
    return m_NewThreadTitle;
}


// 文字列をShift-JISにエンコードする
QString Poster::convertToShiftJIS(const QString &input)
{
    QString result;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QStringEncoder encoder("Shift-JIS");
    QStringDecoder decoder("Shift-JIS");

    // 文字列を1文字ずつ処理
    for (int i = 0; i < input.length(); i++) {
        QChar currentChar = input.at(i);

        // 1文字をShift-JISに変換を試みる
        QByteArray encoded = encoder.encode(QString(currentChar));

        // 変換結果をデコードして元の文字と比較
        QString decoded = decoder.decode(encoded);

        if (!encoder.hasError() && !decoded.isEmpty() && decoded[0] == currentChar) {
            // Shift-JISに変換可能な場合はそのまま追加
            result.append(currentChar);
        }
        else {
            // 変換できない場合は文字参照 (&#xHHHH;) 形式に変換
            result.append(QString("&#x%1;").arg(currentChar.unicode(), 4, 16, QChar('0')));
        }
    }
#else
    QTextCodec *codecShiftJIS = QTextCodec::codecForName("Shift-JIS");

    // 文字列を1文字ずつ処理
    for (int i = 0; i < input.length(); i++) {
        QChar currentChar = input.at(i);

        // 現在の文字をShift-JISに変換を試みる
        QByteArray encoded = codecShiftJIS->fromUnicode(currentChar);

        // 変換結果をデコードして元の文字と比較
        QString decoded = codecShiftJIS->toUnicode(encoded);
        if (decoded.isEmpty() || decoded[0] != currentChar) {
            // 変換できない場合は文字参照（&#xHHHH;）形式に変換
            result.append(QString("&#x%1;").arg(currentChar.unicode(), 4, 16, QChar('0')));
        }
        else {
            // Shift-JISに変換可能な場合はそのまま追加
            result.append(currentChar);
        }
    }
#endif

    return result;
}


// Shift-JISバイト列として取得する
QByteArray Poster::getShiftJISBytes(const QString &input)
{
    QString converted = convertToShiftJIS(input);

    QByteArray encodeArray;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QStringEncoder encoder("Shift-JIS");
    encodeArray = encoder.encode(converted);
#else
    QTextCodec* codecShiftJIS = QTextCodec::codecForName("Shift-JIS");
    encodeArray = codecShiftJIS->fromUnicode(converted);
#endif

    return encodeArray;
}


QString Poster::convertNonSjisToReference(const QString &input)
{
    QString result;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QStringEncoder sjisEncoder(QStringConverter::Encoding::Shift_JIS);
    QStringDecoder sjisDecoder(QStringConverter::Encoding::Shift_JIS);
#else
    QTextCodec* sjisCodec = QTextCodec::codecForName("Shift-JIS");
#endif

    // 入力文字列を1文字ずつ処理
    for (const QChar& ch : input) {
        QString currentChar(ch);
        bool canConvertToSjis = false;

        try {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            // Qt 6
            QByteArray sjisEncoded = sjisEncoder.encode(currentChar);
            if (!sjisEncoder.hasError()) {
                QString roundTrip = sjisDecoder.decode(sjisEncoded);
                if (!sjisDecoder.hasError()) {
                    canConvertToSjis = (roundTrip == currentChar);
                }
            }
#else
            // Qt 5
            QByteArray sjisEncoded = sjisCodec->fromUnicode(currentChar);
            QString roundTrip = sjisCodec->toUnicode(sjisEncoded);
            canConvertToSjis = (roundTrip == currentChar);
#endif
        }
        catch (...) {
            canConvertToSjis = false;
        }

        if (canConvertToSjis) {
            // Shift-JISに変換可能な場合は、そのままUTF-8として追加
            result.append(currentChar);
        }
        else {
            // Shift-JISに変換できない場合は、文字参照形式に変換
            result.append(QString("&#x%1;").arg(ch.unicode(), 4, 16, QChar('0')));
        }
    }

    return result;

    // 異体字セレクタ実装用 (現在は未使用)
//     QString result;

// #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
//     QStringEncoder sjisEncoder(QStringConverter::Encoding::Shift_JIS);
//     QStringDecoder sjisDecoder(QStringConverter::Encoding::Shift_JIS);
// #else
//     QTextCodec* sjisCodec = QTextCodec::codecForName("Shift-JIS");
// #endif

//     // 入力文字列を1文字ずつ処理
//     int i = 0;
//     while (i < input.length()) {
//         bool isFC = false;

//         // 0xFCのシーケンスをチェック
// #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
//         QByteArray encoded = sjisEncoder.encode(input.mid(i, 1));
//         if (!sjisEncoder.hasError() && !encoded.isEmpty() &&
//             static_cast<unsigned char>(encoded[0]) == 0xFC) {
//             isFC = true;
//         }
// #else
//         QByteArray encoded = sjisCodec->fromUnicode(input.mid(i, 1));
//         if (!encoded.isEmpty() && static_cast<unsigned char>(encoded[0]) == 0xFC) {
//             isFC = true;
//         }
// #endif

//         if (isFC) {
//             // 0xFCの場合は置換文字列を追加
//             //result.append("希望の置換文字列を設定");
//             i++;
//         }
//         else {
//             QString currentChar(input[i]);
//             bool canConvertToSjis = false;

//             try {
// #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
//                 QByteArray sjisEncoded = sjisEncoder.encode(currentChar);
//                 if (!sjisEncoder.hasError()) {
//                     QString roundTrip = sjisDecoder.decode(sjisEncoded);
//                     if (!sjisDecoder.hasError()) {
//                         canConvertToSjis = (roundTrip == currentChar);
//                     }
//                 }
// #else
//                 QByteArray sjisEncoded = sjisCodec->fromUnicode(currentChar);
//                 QString roundTrip = sjisCodec->toUnicode(sjisEncoded);
//                 canConvertToSjis = (roundTrip == currentChar);
// #endif
//             }
//             catch (...) {
//                 canConvertToSjis = false;
//             }

//             if (canConvertToSjis) {
//                 result.append(currentChar);
//             }
//             else {
//                 result.append(QString("&#x%1;").arg(currentChar[0].unicode(), 4, 16, QChar('0')));
//             }

//             i++;
//         }
//     }

//     return result;
}


// 特定の文字をURLエンコードする
QString Poster::urlEncode(const QString &originalString)
{
    // URLエンコードする文字
    QVector<QString> toEncode = {"+", "&", "#", "=", "|", "[", "]", "{", "}", "'", "\"", "<", ">", " "};
    QVector<QString> encoded;

    // 置換処理
    for (const auto &str : qAsConst(toEncode)) {
        encoded.append(QUrl::toPercentEncoding(str));
    }

    QString encodedString = originalString;
    for (int i = 0; i < toEncode.size(); i++) {
        encodedString.replace(toEncode[i], encoded[i]);
    }

    return encodedString;
}
