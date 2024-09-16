#include <QFileInfo>
#include <QDir>
#include <QLockFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QTimeZone>
#include <QException>
#include <iostream>
#include "WriteMode.h"
#include "HtmlFetcher.h"
#include "Poster.h"


// 静的メンバの初期化
WriteMode*  WriteMode::m_instance = nullptr;
QMutex      WriteMode::m_confMutex;
QMutex      WriteMode::m_logMutex;


WriteMode::WriteMode(QObject *parent) : QObject{parent}, m_Article(nullptr)
{
}


WriteMode::~WriteMode()
{
}


// シングルトンインスタンスを取得するための静的メソッド
WriteMode* WriteMode::getInstance()
{
    if (m_instance == nullptr) {
        QMutexLocker confLocker(&m_confMutex);
        QMutexLocker logLocker(&m_logMutex);

        if (m_instance == nullptr) {
            m_instance = new WriteMode();
        }
    }

    return m_instance;
}


// qNewsFlashの設定ファイルを指定
void WriteMode::setSysConfFile(const QString &confFile)
{
    m_SysConfFile = confFile;
}


// 書き込みに成功したニュース記事の情報を保存するログファイルを指定
void WriteMode::setLogFile(const QString &logFile)
{
    m_LogFile = logFile;
}


// 書き込むニュース記事を指定
void WriteMode::setArticle(const Article &object)
{
    m_Article = object;
}


// 書き込むニュース記事を指定
void WriteMode::setArticle(const QString &title, const QString &paragraph, const QString &link, const QString &pubDate)
{
    m_Article = Article(title, paragraph, link, pubDate);
}


// 書き込むスレッド情報を指定
void WriteMode::setThreadInfo(const THREAD_INFO &object)
{
    m_ThreadInfo = object;
}


// スレッドの書き込みに必要な情報を指定
void WriteMode::setWriteInfo(const WRITE_INFO &object)
{
    m_WriteInfo = object;
}


// 書き込むスレッド情報を取得
THREAD_INFO WriteMode::getThreadInfo() const
{
    return m_ThreadInfo;
}


// スレッドの書き込みに必要な情報を取得
WRITE_INFO WriteMode::getWriteInfo() const
{
    return m_WriteInfo;
}


// 書き込みモード 1 : 1つのスレッドにニュース記事および時事ドットコムの速報ニュースを書き込むモード
int WriteMode::writeMode1()
{
    // ニュース記事のタイトル --> 公開日 --> 本文の一部 --> URL の順に並べて書き込む
    // ただし、ニュース記事の本文を取得しない場合は、ニュース記事のタイトル --> 公開日 --> URL の順とする
    auto [title, paragraph, link, pubDate] = m_Article.getArticleData();

    m_ThreadInfo.message = QString("%1%2%3%4").arg(!title.isEmpty()        ? title + "\n"     : "",
                                                   !pubDate.isEmpty()      ? pubDate + "\n\n" : "",
                                                   paragraph.length() == 0 ? ""               : paragraph + "\n",
                                                   link);

    // 現在時刻をエポックタイムで取得
    auto epocTime = WriteMode::getEpocTime();
    m_ThreadInfo.time = QString::number(epocTime);

    // スレッドのタイトルを抽出するためのXPath
    // これは、新規スレッド作成向け、および、!chttコマンド向けの設定
    m_ThreadInfo.expiredXPath = m_WriteInfo.ExpiredXpath;

    Poster poster(this);

    // 掲示板のクッキーを取得
    if (poster.fetchCookies(QUrl(m_WriteInfo.RequestURL))) {
        // クッキーの取得に失敗した場合
        return WRITEERROR::POSTERROR;
    }

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR >= 1) && (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR <= 2)
    // qNewsFlash 0.1.0から0.2.xまでの機能

    // スレッドのタイトルを変更するかどうか (防弾嫌儲およびニュース速報(Libre)等の掲示板で使用可能)
    if (m_WriteInfo.ChangeTitle) m_ThreadInfo.message.prepend("!chtt");

    // 既存のスレッドに書き込み
    if (poster.PostforWriteThread(QUrl(m_WriteInfo.RequestURL), m_ThreadInfo)) {
        // 既存のスレッドへの書き込みに失敗した場合
        return -1;
    }
#elif QNEWSFLASH_VERSION_MAJOR > 0 || (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR >= 3)
    // qNewsFlash 0.3.0以降の機能

    // 指定のスレッドがレス数の上限に達して書き込めない場合、スレッドを新規作成する
    HtmlFetcher fetcher(this);

    // 設定ファイルにあるスレッドのURLが生存しているかどうかを確認
    auto iExpired = fetcher.checkUrlExistence(QUrl(m_WriteInfo.ThreadURL), m_WriteInfo.ThreadTitle, m_WriteInfo.ExpiredXpath, m_ThreadInfo.shiftjis);

    if (iExpired == 0) {
        // 設定ファイルにあるスレッドのURLが生存している場合

        // 書き込むスレッドのレス数が上限に達しているかどうかを確認
        auto ret = checkLastThreadNum();
        if (ret == -1) {
            // 最後尾のレス番号の取得に失敗した場合
            std::cerr << QString("エラー : レス数の取得に失敗").toStdString() << std::endl;
            return WRITEERROR::POSTERROR;
        }
        else if (ret == 1) {
            // 既存のスレッドが最大レス数に達している場合、スレッドの新規作成

            // 設定ファイルの"subject"キーの値が空欄の場合、ニュース記事のタイトルをスレッドのタイトルにする
            // "subject"キーの値に%tトークンが存在する場合はニュース記事のタイトルに置換
            if (m_ThreadInfo.subject.isEmpty()) {
                m_ThreadInfo.subject = title;
            }
            else {
                auto newTitle = replaceSubjectToken(m_ThreadInfo.subject, title);
                m_ThreadInfo.subject = !newTitle.isEmpty() ? newTitle : title;
            }

            if (poster.PostforCreateThread(QUrl(m_WriteInfo.RequestURL), m_ThreadInfo)) {
                // スレッドの新規作成に失敗した場合
                return WRITEERROR::POSTERROR;
            }

            // 新規作成したスレッドのタイトル、URL、スレッド番号を取得
            m_WriteInfo.ThreadURL   = poster.GetNewThreadURL();
            m_ThreadInfo.key        = poster.GetNewThreadNum();
            m_WriteInfo.ThreadTitle = poster.GetNewThreadTitle();

            // スレッド情報 (スレッドのタイトル、URL、スレッド番号) を設定ファイルに保存
            {
                QMutexLocker confLocker(&m_confMutex);
                if (updateThreadJson(m_WriteInfo.ThreadTitle)) {
                    // スレッド情報の保存に失敗
                    return WRITEERROR::POSTERROR;
                }
            }
        }
        else {
            // 既存のスレッドが存在する場合

            // スレッドのタイトルを変更するかどうか (防弾嫌儲およびニュース速報(Libre)等の掲示板で使用可能)
            if (m_WriteInfo.ChangeTitle) m_ThreadInfo.message.prepend("!chtt");

            // 既存のスレッドに書き込み
            if (poster.PostforWriteThread(QUrl(m_WriteInfo.RequestURL), m_ThreadInfo)) {
                // 既存のスレッドへの書き込みに失敗した場合
                return WRITEERROR::POSTERROR;
            }

            // スレッドのタイトルが正常に変更されているかどうかを確認
            if (m_WriteInfo.ChangeTitle) {
                // !chttコマンドが有効の場合
                if (CompareThreadTitle(QUrl(m_WriteInfo.ThreadURL), m_WriteInfo.ThreadTitle) == 0) {
                    // スレッドのタイトルが正常に変更された場合
                    // スレッド情報 (スレッドのタイトル、スレッドのURL、スレッド番号) を設定ファイルに保存
                    {
                        QMutexLocker confLocker(&m_confMutex);
                        if (updateThreadJson(m_WriteInfo.ThreadTitle)) {
                            // スレッド情報の保存に失敗
                            return WRITEERROR::POSTERROR;
                        }
                    }
                }
                else {
                    // スレッドのタイトルが変更されなかった場合
                    // スレッド情報 (スレッドのURLおよびスレッド番号) を設定ファイルに保存
                    {
                        QMutexLocker confLocker(&m_confMutex);
                        if (updateThreadJson(m_WriteInfo.ThreadTitle)) {
                            // スレッド情報の保存に失敗
                            return WRITEERROR::POSTERROR;
                        }
                    }
                }
            }
            else {
                // !chttコマンドが無効の場合
                // スレッド情報 (スレッドのURLおよびスレッド番号) を設定ファイルに保存
                {
                    QMutexLocker confLocker(&m_confMutex);
                    if (updateThreadJson(m_WriteInfo.ThreadTitle)) {
                        // スレッド情報の保存に失敗
                        return WRITEERROR::POSTERROR;
                    }
                }
            }
        }
    }
    else if (iExpired == 1) {
        // 設定ファイルにあるスレッドのURLが存在しない場合
        // または、スレッドタイトルが異なる場合
        // (スレッドを新規作成する)

        // 設定ファイルの"subject"キーの値が空欄の場合、ニュース記事のタイトルをスレッドのタイトルにする
        // "subject"キーの値に%tトークンが存在する場合はニュース記事のタイトルに置換
        if (m_ThreadInfo.subject.isEmpty()) {
            m_ThreadInfo.subject = title;
        }
        else {
            auto newTitle = replaceSubjectToken(m_ThreadInfo.subject, title);
            m_ThreadInfo.subject = !newTitle.isEmpty() ? newTitle : title;
        }

        // スレッドの新規作成
        if (poster.PostforCreateThread(QUrl(m_WriteInfo.RequestURL), m_ThreadInfo)) {
            // スレッドの新規作成に失敗した場合
            return -1;
        }

        // 新規作成したスレッドのタイトル、URL、スレッド番号を取得
        m_WriteInfo.ThreadURL      = poster.GetNewThreadURL();
        m_ThreadInfo.key           = poster.GetNewThreadNum();
        m_WriteInfo.ThreadTitle    = poster.GetNewThreadTitle();

        // スレッド情報 (スレッドのタイトル、URL、スレッド番号) を設定ファイルに保存
        {
            QMutexLocker confLocker(&m_confMutex);
            if (updateThreadJson(m_WriteInfo.ThreadTitle)) {
                // スレッド情報の保存に失敗
                return -1;
            }
        }
    }
    else {
        return -1;
    }
#endif

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
    // qNewsFlash 0.1.0未満の機能
    // JSONファイル(スレッド書き込み用)に書き込む
    // 該当スレッドへの書き込み処理は各自で実装する
    if (writeJSON(m_Article)) {
        return WRITEERROR::LOGERROR;
    }
#endif

    // ログファイルに書き込む
    {
        QMutexLocker logLocker(&m_logMutex);
        if (writeLog(m_Article, m_WriteInfo.ThreadTitle, m_WriteInfo.ThreadURL, m_ThreadInfo.key)) {
            return WRITEERROR::LOGERROR;
        }
    }

    return WRITEERROR::SUCCEED;
}


// 書き込みモード 2 : ニュース記事および時事ドットコムの速報ニュースにおいて、常に新規スレッドを立てるモード
int WriteMode::writeMode2()
{
    // ニュース記事のタイトル --> 公開日 --> 本文の一部 --> URL の順に並べて書き込む
    // ただし、ニュース記事の本文を取得しない場合は、ニュース記事のタイトル --> 公開日 --> URL の順とする
    auto [title, paragraph, link, pubDate] = m_Article.getArticleData();

    THREAD_INFO tInfo;

    // 設定ファイルの"subject"キーの値が空欄の場合、ニュース記事のタイトルをスレッドのタイトルにする
    // "subject"キーの値に%tトークンが存在する場合はニュース記事のタイトルに置換
    // 現在の仕様では、書き込みモード 2の場合は、この機能を無効とする
    //if (m_ThreadInfo.subject.isEmpty()) {
    //    tInfo.subject = title;
    //}
    //else {
    //    auto newTitle = replaceSubjectToken(m_ThreadInfo.subject, title);
    //    tInfo.subject = !newTitle.isEmpty() ? newTitle : title;
    //}

    tInfo.subject       = title;
    tInfo.from          = m_ThreadInfo.from;
    tInfo.mail          = m_ThreadInfo.mail;
    tInfo.message       = QString("%1%2%3").arg(!pubDate.isEmpty()  ? pubDate + "\n\n" : "",
                                            paragraph.length() == 0 ? ""               : paragraph + "\n",
                                            link);
    tInfo.bbs           = m_ThreadInfo.bbs;
    tInfo.shiftjis      = m_ThreadInfo.shiftjis;

    // 現在時刻をエポックタイムで取得
    auto epocTime = WriteMode::getEpocTime();
    tInfo.time = QString::number(epocTime);

    // スレッドのタイトルを抽出するためのXPath
    // これは、新規スレッド作成向け、および、!chttコマンド向けの設定
    tInfo.expiredXPath = m_WriteInfo.ExpiredXpath;

    Poster poster(this);

    // 掲示板のクッキーを取得
    if (poster.fetchCookies(QUrl(m_WriteInfo.RequestURL))) {
        // クッキーの取得に失敗した場合
        return WRITEERROR::POSTERROR;
    }

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR >= 1) && (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR <= 2)
    // qNewsFlash 0.1.0から0.2.xまでの機能

    // スレッドのタイトルを変更するかどうか (防弾嫌儲およびニュース速報(Libre)等の掲示板で使用可能)
    if (m_WriteInfo.ChangeTitle) m_ThreadInfo.message.prepend("!chtt");

    // 既存のスレッドに書き込み
    if (poster.PostforWriteThread(QUrl(m_WriteInfo.RequestURL), m_ThreadInfo)) {
        // 既存のスレッドへの書き込みに失敗した場合
        return -1;
    }
#elif QNEWSFLASH_VERSION_MAJOR > 0 || (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR >= 3)
    // qNewsFlash 0.3.0以降の機能

    // スレッドを新規作成する
    HtmlFetcher fetcher(this);

    // スレッドの新規作成
    if (poster.PostforCreateThread(QUrl(m_WriteInfo.RequestURL), tInfo)) {
        // スレッドの新規作成に失敗した場合
        return -1;
    }

    // 新規作成したスレッドのタイトル、URL、スレッド番号を取得
    auto threadtitle = poster.GetNewThreadTitle();
    auto threadurl   = poster.GetNewThreadURL();
    tInfo.key        = poster.GetNewThreadNum();

#endif

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
    // qNewsFlash 0.1.0未満の機能
    // JSONファイル(スレッド書き込み用)に書き込む
    // 該当スレッドへの書き込み処理は各自で実装する
    if (writeJSON(m_Article)) {
        return WRITEERROR::LOGERROR;
    }
#endif

    // ログファイルに書き込む
    {
        QMutexLocker logLocker(&m_logMutex);
        if (writeLog(m_Article, threadtitle, threadurl, tInfo.key, true)) {
            return WRITEERROR::LOGERROR;
        }
    }

    return WRITEERROR::SUCCEED;
}


// 現在のエポックタイム (UNIX時刻) を秒単位で取得する
qint64 WriteMode::getEpocTime()
{
    // 現在の日時を取得
    QDateTime now = QDateTime::currentDateTime().toTimeZone(QTimeZone("Asia/Tokyo"));;

    // エポックタイム (UNIX時刻) を秒単位で取得
    qint64 epochTime = now.toSecsSinceEpoch();

    return epochTime;
}


// 現在の日本時刻を"yyyy年M月d日 H時m分"形式で取得
QString WriteMode::getCurrentTime()
{
    // 日本のタイムゾーンを設定
    QTimeZone japanTimeZone("Asia/Tokyo");

    // 現在の日本時間を取得
    QDateTime japanTime = QDateTime::currentDateTime().toTimeZone(japanTimeZone);

    // 日本語ロケールを設定
    QLocale japaneseLocale(QLocale::Japanese, QLocale::Japan);

    // 指定された形式で日時を文字列に変換
    QString formattedTime = japaneseLocale.toString(japanTime, "yyyy年M月d日 H時m分");

    return formattedTime;
}


// 文字列 %t をスレッドのタイトルへ変更
QString WriteMode::replaceSubjectToken(QString subject, QString title)
{
    // エスケープされた"%t"トークンを一時的に保護
    QString tempToken = "__TEMP__";
    subject.replace("%%t", tempToken);

    int count = subject.count("%t");
    if (count > 1) {
        std::cout << QString("警告 : \"%t\"トークンが複数存在します").toStdString() << std::endl;
                     subject.replace(tempToken, "%%t");

        return QString();
    }
    else if (count == 1) {
        // "%t"トークンをスレッドのタイトルに置換
        subject.replace("%t", title);
    }

    // 保護されたトークンを元に戻す
    subject.replace(tempToken, "%%t");

    return subject;
}


// 書き込むスレッドのレス数が上限に達しているかどうかを確認
int WriteMode::checkLastThreadNum()
{
    HtmlFetcher fetcher(this);
    if (fetcher.fetchLastThreadNum(QUrl(m_WriteInfo.ThreadURL), false, m_WriteInfo.ThreadXPath, XML_TEXT_NODE)) {
        /// 最後尾のレス番号の取得に失敗した場合
        return -1;
    }
    auto element = fetcher.GetElement();

    bool ok;
    auto max = element.toInt(&ok);
    if (!ok) {
        return -1;
    }

    // スレッドのレス数が上限に達したかどうかを確認
    if (max >= m_WriteInfo.MaxThreadNum) {
        // 上限に達している場合
        return 1;
    }

    return 0;
}


// 書き込むスレッドのレス数を取得
int WriteMode::getLastThreadNum(const WRITE_INFO &wInfo)
{
    HtmlFetcher fetcher(this);
    if (fetcher.fetchLastThreadNum(QUrl(wInfo.ThreadURL), false, wInfo.ThreadXPath, XML_TEXT_NODE)) {
        /// 最後尾のレス番号の取得に失敗した場合
        return -1;
    }
    auto element = fetcher.GetElement();

    bool ok;
    auto max = element.toInt(&ok);
    if (!ok) {
        return -1;
    }

    return max;
}


// !chttコマンドでスレッドのタイトルが正常に変更されているかどうかを判断する
// !chttコマンドは、防弾嫌儲系のみ使用可能
// 0  : スレッドのタイトルが正常に変更された場合
// 1  : !chttコマンドが失敗している場合
// -1 : スレッドのタイトルの取得に失敗した場合
int WriteMode::CompareThreadTitle(const QUrl &url, QString &title)
{
    // スレッドから<title>タグをXPathを使用して抽出する
    HtmlFetcher fetcher(this);
    if (fetcher.extractThreadTitle(url, true, m_WriteInfo.ExpiredXpath, m_ThreadInfo.shiftjis)) {
        // <title>タグの取得に失敗した場合
        std::cerr << QString("エラー : <title>タグの取得に失敗 - CompareThreadTitle()").toStdString() << std::endl;
        return -1;
    }

    auto ThreadTitle = fetcher.GetElement();

    // 正規表現を定義（スペースを含む [ と ] の間に任意の文字列があるパターン）
    // (現在は使用しない)
    //static const QRegularExpression RegEx(" \\[.*\\]$");

    // 文字列からパターンに一致する部分を削除
    // (現在は使用しない)
    //ThreadTitle = ThreadTitle.remove(RegEx);

    if (ThreadTitle.compare(title, Qt::CaseSensitive) == 0) {
        // スレッドのタイトルが変更されていない場合 (!chttコマンドが失敗している場合)
        return 1;
    }
    else {
        // スレッドのタイトルが変更された場合 (!chttコマンドが成功した場合)
        title = ThreadTitle;
    }

    return 0;
}


// スレッド情報 (スレッドのタイトル、スレッドのURL、スレッド番号) を設定ファイルに保存
int WriteMode::updateThreadJson(const QString &title)
{
    QFileInfo configFileInfo(m_SysConfFile);
    QString   lockFilePath = configFileInfo.dir().filePath(configFileInfo.baseName() + ".lock");
    QLockFile lockFile(lockFilePath);

    // 最大30秒の間に、システムは繰り返しロックの取得を試みる
    if (!lockFile.tryLock(30000)) {
        std::cerr << QString("エラー: 30秒以内に書き込み用ログファイルのロックの取得に失敗").toStdString() << std::endl;
        return -1;
    }

    // 設定ファイルの読み込み
    QFile File(m_SysConfFile);
    if (!File.open(QIODevice::ReadWrite)) {
        lockFile.unlock();
        std::cerr << QString("エラー : 設定ファイルのオープンに失敗 %1").arg(m_SysConfFile).toStdString() << std::endl;

        return -1;
    }

    QJsonDocument doc;
    try {
        // 設定ファイルの内容を読み込む
        doc = QJsonDocument::fromJson(File.readAll());
        if (!doc.isObject()) {
            if (File.isOpen())  File.close();
            lockFile.unlock();
            std::cerr << QString("エラー : 設定ファイルがオブジェクトではありません").toStdString() << std::endl;

            return -1;
        }

        QJsonObject obj = doc.object();

        // 設定ファイルの"thread"オブジェクトにある"key"および"threadurl"を更新
        QJsonObject threadObj       = obj.value("thread").toObject();
        threadObj["key"]            = m_ThreadInfo.key;
        threadObj["threadurl"]      = m_WriteInfo.ThreadURL;
        threadObj["threadtitle"]    = title;
        obj["thread"]               = threadObj;

        doc.setObject(obj);

        // ファイルの先頭に戻る
        File.seek(0);

        // 更新されたJSONオブジェクトをファイルに書き込む
        File.write(doc.toJson());
        File.resize(File.pos());    // ファイルサイズを現在の位置に切り詰める
        File.flush();               // 確実にディスクに書き込む
    }
    catch (QException &ex) {
        if (File.isOpen())  File.close();
        lockFile.unlock();
        std::cerr << QString("エラー : %1").arg(ex.what()).toStdString() << std::endl;

        return -1;
    }

    // 設定ファイルを閉じる
    File.close();

    // ロックファイルを削除
    lockFile.unlock();

    return 0;
}


// 書き込み済みのニュース記事をJSONファイルに保存
int WriteMode::writeLog(Article &article, const QString &threadtitle, const QString &threadurl, const QString &key, bool bNewThread)
{
    QFileInfo logFileInfo(m_LogFile);
    QString   lockFilePath = logFileInfo.dir().filePath(logFileInfo.baseName() + ".lock");
    QLockFile lockFile(lockFilePath);

    // 最大30秒の間に、システムは繰り返しロックの取得を試みる
    if (!lockFile.tryLock(30000)) {
        std::cerr << QString("エラー: ログファイルのロックの取得に失敗").toStdString() << std::endl;
        return -1;
    }

    QFile File(m_LogFile);

    try {
        // ログファイルを開く
        if (!File.open(QIODevice::ReadWrite)) {
            throw std::runtime_error(QString("エラー: ログファイルのオープンに失敗  %1").arg(File.errorString()).toStdString());
        }

        // ログファイルを読み込む
        QJsonDocument jsonDoc;
        QByteArray jsonData = File.readAll();
        if (!jsonData.isEmpty()) {
            QJsonParseError jsonError;
            jsonDoc = QJsonDocument::fromJson(jsonData, &jsonError);
            if (jsonDoc.isNull()) {
                throw std::runtime_error(jsonError.errorString().toStdString());
            }
        }

        if (!jsonDoc.isArray()) {
            jsonDoc.setArray(QJsonArray());
        }

        auto jsonArray = jsonDoc.array();
        auto [title, paragraph, url, date] = article.getArticleData();
        QJsonObject newObject;

        // ニュース記事の情報をログファイルに保存
        newObject["title"]      = title;
        newObject["paragraph"]  = paragraph;
        newObject["url"]        = url;
        newObject["date"]       = date;

        // スレッドの情報をログファイルに保存
        QJsonObject threadObject;
        threadObject["title"]   = threadtitle;                      // スレッドのタイトル
        threadObject["url"]     = threadurl;                        // スレッドのURL
        threadObject["key"]     = key;                              // スレッド番号
        auto currentDate        = WriteMode::getCurrentTime();      // スレッドの作成日時
        threadObject["time"]    = currentDate;
        threadObject["new"]     = bNewThread;                       // ニュース記事を新規スレッドで立てているかどうか
        threadObject["bottom"]  = false;                            // !bottomコマンド ("書き込みモード 2", "書き込みモード 3の一般ニュース"の場合のみ、このフラグを使用)

        newObject["thread"] = threadObject;

        jsonArray.append(newObject);

        File.seek(0);
        File.resize(0);
        QByteArray newJsonData = QJsonDocument(jsonArray).toJson();
        if (File.write(newJsonData) != newJsonData.size()) {
            throw std::runtime_error(File.errorString().toStdString());
        }

        File.flush();

        // !bottomコマンド機能を有効にしている場合
        // 新規スレッドを立てる場合のみ、!bottomコマンド機能向けに書き込みログを保存
        // 書き込みモード 2 または 書き込みモード 3の一般ニュースのみ
        if (bNewThread && m_WriteInfo.BottomThread) {
            WRITE_LOG log {
                .Title    = threadtitle,    // スレッドのタイトル
                .Url      = threadurl,      // スレッドのURL
                .Key      = key,            // スレッド番号
                .Time     = currentDate,    // スレッドの作成日時
                .bottom   = false           // !bottomコマンド
            };
            m_WriteLogs.append(log);
        }
    }
    catch (const std::runtime_error &e) {
        if (File.isOpen())          File.close();
        if (lockFile.isLocked())    lockFile.unlock();
        std::cerr << QString("エラー: %1").arg(e.what()).toStdString() << std::endl;

        return -1;
    }
    catch (const std::exception &e) {
        if (File.isOpen())          File.close();
        if (lockFile.isLocked())    lockFile.unlock();
        std::cerr << QString("エラー: %1").arg(e.what()).toStdString() << std::endl;

        return -1;
    }

    // ログファイルを閉じる
    File.close();

    // ロックファイルを削除
    lockFile.unlock();

    return 0;
}


// 最後にニュース記事を取得した日付を設定ファイルに保存 (フォーマット : "yyyy/M/d")
int WriteMode::updateDateJson(const QString &currentDate)
{
    QMutexLocker confLocker(&m_confMutex);

    return updateDateJsonWrapper(currentDate);
}


// (ラッパー向け) 最後にニュース記事を取得した日付を設定ファイルに保存 (フォーマット : "yyyy/M/d")
int WriteMode::updateDateJsonWrapper(const QString &currentDate)
{
    QFileInfo configFileInfo(m_SysConfFile);
    QString lockFilePath = configFileInfo.dir().filePath(configFileInfo.baseName() + ".lock");
    QLockFile lockFile(lockFilePath);

    // 最大30秒の間に、システムは繰り返しロックの取得を試みる
    if (!lockFile.tryLock(30000)) {
        std::cerr << QString("エラー: 30秒以内に書き込み用ログファイルのロックの取得に失敗").toStdString() << std::endl;
        return -1;
    }

    QJsonDocument doc;
    QFile File(m_SysConfFile);

    // 設定ファイルを開く
    if (!File.open(QIODevice::ReadWrite)) {
        lockFile.unlock();
        std::cerr << QString("エラー : 設定ファイルのオープンに失敗 %1").arg(m_SysConfFile).toStdString() << std::endl;

        return -1;
    }

    try {
        // 設定ファイルの内容を読み込む
        doc = QJsonDocument::fromJson(File.readAll());

        if (!doc.isObject()) {
            throw std::runtime_error("設定ファイルが有効なJSONオブジェクトではありません");
        }

        QJsonObject obj = doc.object();

        // 設定ファイルの"update"キーを更新
        obj["update"] = currentDate;
        doc.setObject(obj);

        // ファイルポインタを先頭に戻す
        File.seek(0);

        // 更新したJSONオブジェクトをファイルに書き込む
        File.write(doc.toJson());
        File.flush();
        File.resize(File.pos());  // ファイルサイズを現在の位置に切り詰める
    }
    catch(const std::exception &ex) {
        if (File.isOpen())       File.close();
        if (lockFile.isLocked()) lockFile.unlock();
        std::cerr << QString("エラー : %1").arg(ex.what()).toStdString() << std::endl;

        return -1;
    }

    // 設定ファイルを閉じる
    File.close();

    // ロックファイルを削除
    lockFile.unlock();

    return 0;
}


#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
// qNewsFlash 0.1.0未満の機能
int WriteMode::writeJSON(Article &article)
{
    auto [title, paragraph, url, date] = article.getArticleData();

    // JSONオブジェクトの作成
    QJsonObject jsonObject;

    /// 書き込む記事のタイトル
    jsonObject["title"] = title;

    /// 書き込む記事の本文の一部
    jsonObject["paragraph"] = paragraph;

    /// 書き込む記事のURL
    jsonObject["url"] = url;

    /// 書き込む記事の公開日
    jsonObject["date"] = date;

    // JSONドキュメントを作成
    QJsonDocument jsonDocument(jsonObject);

    try {
        // JSONファイルを作成
        QFile file(m_WriteFile);
        if (!file.open(QIODevice::WriteOnly)) {
            std::cerr << QString("エラー : ファイルのオープンに失敗 %1").arg(file.errorString()).toStdString() << std::endl;
            return -1;
        }

        file.write(jsonDocument.toJson());
        file.close();
    }
    catch (QException &ex) {
        std::cerr << QString("エラー : JSONファイルの作成に失敗 %1").arg(ex.what()).toStdString() << std::endl;
        return -1;
    }

    return 0;
}


int WriteMode::truncateJSON()
{
    try {
        // JSONファイルを作成
        QFile File(m_WriteFile);
        if (!File.open(QIODevice::WriteOnly)) {
            std::cerr << QString("エラー : ファイルのオープンに失敗 %1").arg().toStdString(file.errorString()) << std::endl;
            return -1;
        }

        File.close();
    }
    catch (QException &ex) {
        std::cerr << QString("エラー : JSONファイルの作成に失敗 %1").arg(ex.what()).toStdString() << std::endl;
        return -1;
    }

    return 0;
}
#endif


// ログ情報を保存するファイルから、昨日以前(昨日も含む)の書き込み済みのニュース記事を削除
int WriteMode::deleteLogNotToday()
{
    QMutexLocker logLocker(&m_logMutex);

    return deleteLogNotTodayWrapper();
}


// (ラッパー向け) ログ情報を保存するファイルから、昨日以前(昨日も含む)の書き込み済みのニュース記事を削除
int WriteMode::deleteLogNotTodayWrapper()
{
    QFileInfo logFileInfo(m_LogFile);
    QString   lockFilePath = logFileInfo.dir().filePath(logFileInfo.baseName() + ".lock");
    QLockFile lockFile(lockFilePath);

    // 最大30秒の間に、システムは繰り返しロックの取得を試みる
    if (!lockFile.tryLock(30000)) {
        std::cerr << QString("エラー: 30秒以内に書き込み用ログファイルのロックの取得に失敗").toStdString() << std::endl;
        return -1;
    }

    // ログファイルを開く
    QFile File(m_LogFile);
    if (!File.open(QIODevice::ReadWrite)) {
        if (lockFile.isLocked())    lockFile.unlock();
        std::cerr << QString("エラー: ログファイルのオープンに失敗").toStdString() << std::endl;

        return -1;
    }

    try {
        // ログファイルの読み込み
        QByteArray data = File.readAll();

        // 公開日が2日以上前の書き込み済みニュース記事をログファイルから削除
        QJsonDocument doc(QJsonDocument::fromJson(data));
        QJsonArray array = doc.array();

        // 保存する書き込み済み記事
        QJsonArray newLogArray;

        // 日本のタイムゾーンを設定して、今日の日付を取得
        QTimeZone timeZone("Asia/Tokyo");
        QDate currentDate = QDate::currentDate();

        for (const auto &value : array) {
            auto obj = value.toObject();

            /// " h時m分"の部分を除去
            auto pubDateString = obj["date"].toString().split(" ")[0];
            auto pubDate = QDate::fromString(pubDateString, "yyyy年M月d日");

            /// 現在の日付と公開日の日付の差
            auto daysDiff = pubDate.daysTo(currentDate);

            /// ニュース記事の公開日が前日までの場合は残す
            if (daysDiff >= 0 && daysDiff <= 1) {
                newLogArray.append(obj);
            }
        }

        // ファイルポインタを先頭に戻す
        File.seek(0);

        // 更新された内容を書き込む
        QJsonDocument saveDoc(newLogArray);
        if (File.write(saveDoc.toJson()) == -1) {
            throw std::runtime_error("ログファイルの書き込みに失敗");
        }
        if (!File.flush()) {
            throw std::runtime_error("ログファイルのフラッシュに失敗");
        }

        // ファイルサイズを現在の位置に切り詰める
        File.resize(File.pos());
    }
    catch (const std::exception &ex) {
        if (File.isOpen())       File.close();
        if (lockFile.isLocked()) lockFile.unlock();
        std::cerr << "エラー : " << ex.what() << std::endl;

        return -1;
    }

//    if (m_WithinHours == 0) {
//        /// 昨日以前のニュース記事を削除
//        /// 日本のタイムゾーンを設定して、今日の日付を取得
//        auto now = QDateTime::currentDateTimeUtc().toTimeZone(QTimeZone("Asia/Tokyo"));
//        auto currentDate = now.date().toString("yyyy年M月d日");

//        for (auto && i : array) {
//            auto obj = i.toObject();

//            /// " h時m分"の部分を除去
//            auto dateString = obj["date"].toString().split(" ")[0];

//            if (dateString.compare(currentDate) == 0) {
//                newLogArray.append(obj);
//            }
//        }
//    }
//    else if (0 < m_WithinHours) {
//        /// qNewsFlash.jsonの"withinhours"キーに指定した時間以前のニュース記事を削除
//        /// 日本のタイムゾーンを設定して、現在の日時を取得
//        auto currentTime = QDateTime::currentDateTime().toTimeZone(QTimeZone("Asia/Tokyo"));

//        /// 現在時刻からm_WithinHours時間前の時刻を計算
//        QDateTime beforeTime = currentTime.addSecs(- m_WithinHours * 60 * 60);

//        for (auto && i : array) {
//            auto obj = i.toObject();

//            /// ログファイルから各記事の公開日を取得
//            auto articleTime = obj["date"].toString();

//            /// 取得した各記事の公開日の日時形式を変換
//            QDateTime dataTime = QDateTime::fromString(articleTime, "yyyy年M月d日 H時m分");

//            /// m_WithinHours時間内の場合は保存
//            if (dataTime >= beforeTime && dataTime <= currentTime) {
//                newLogArray.append(obj);
//            }
//        }
//    }

    // ログファイルを閉じる
    File.close();

    // ロックファイルを削除
    lockFile.unlock();

    return 0;
}


// ログファイルから、本日の書き込み済みのニュース記事を取得
// また、取得した記事群のデータは、メンバ変数m_WrittenArticlesに保存
// ただし、このメソッドは、deleteLogNotToday()メソッドの直後に実行する必要がある
QList<Article> WriteMode::getDatafromWrittenLog()
{
    QMutexLocker logLocker(&m_logMutex);

    try {
        return getDatafromWrittenLogWrapper();
    }
    catch (const std::runtime_error &e) {
        // 例外を再スローして、呼び出し元に伝播させる
        throw;
    }
    catch (const std::exception &e) {
        // 例外を再スローして、呼び出し元に伝播させる
        throw;
    }

    return QList<Article>{};
}


// (ラッパー向け) ログファイルから、本日の書き込み済みのニュース記事を取得
// また、取得した記事群のデータは、メンバ変数m_WrittenArticlesに保存
// ただし、このメソッドは、deleteLogNotToday()メソッドの直後に実行する必要がある
QList<Article> WriteMode::getDatafromWrittenLogWrapper()
{
    QList<Article> writtenArticles;

    QFileInfo logFileInfo(m_LogFile);
    QString   lockFilePath = logFileInfo.dir().filePath(logFileInfo.baseName() + ".lock");
    QLockFile lockFile(lockFilePath);

    // 最大30秒の間に、システムは繰り返しロックの取得を試みる
    if (!lockFile.tryLock(30000)) {
        std::cerr << QString("エラー: 30秒以内に書き込み用ログファイルのロックの取得に失敗").toStdString() << std::endl;
        throw std::runtime_error("エラー : ログファイルのロック取得に失敗");
    }

    try {
        // ログファイルを開く
        QFile File(m_LogFile);
        if (!File.open(QIODevice::ReadOnly)) {
            lockFile.unlock();
            throw std::runtime_error("エラー : ログファイルのオープンに失敗");
        }

        // ログファイル内の書き込み済みの記事群を読み込み
        QByteArray data = File.readAll();

        // ログファイルを閉じる
        File.close();

        // ログファイル内にある書き込み済みの記事群を取得 (本日分のみ)
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (!jsonDoc.isArray()) {
            lockFile.unlock();
            throw std::runtime_error("エラー : ログファイルのフォーマットが不正です");
        }

        QJsonArray jsonArray = jsonDoc.array();
        for (const auto &value : jsonArray) {
            QJsonObject obj = value.toObject();
            Article article(obj["title"].toString(), obj["paragraph"].toString(), obj["url"].toString(), obj["date"].toString());
            writtenArticles.append(article);
        }
    }
    catch (const QException &ex) {
        lockFile.unlock();
        throw std::runtime_error(QString("エラー : ログファイルの読み込みに失敗 %1").arg(ex.what()).toStdString());
    }
    catch (const std::exception &e) {
        // 既存の例外を再スロー
        lockFile.unlock();
        throw;
    }

    // ロックファイルを削除
    lockFile.unlock();

    return writtenArticles;
}


// 最も早く新規スレッドを立てた書き込み済みスレッド情報を取得
std::optional<WRITE_LOG> WriteMode::getOldestWriteLog() const
{
    if (m_WriteLogs.isEmpty()) {
        return std::nullopt;
    }

    return m_WriteLogs.at(0);
}


// 最も早く新規スレッドを立てた日時を取得
QString WriteMode::getOldestWriteLogDate() const
{
    return m_WriteLogs.length() > 0 ? m_WriteLogs.at(0).Time : QString("");
}


// 任意の時間が過ぎた書き込み済みスレッドに対して、レスが無い場合は!bottomコマンドを書き込む
int WriteMode::writeBottom()
{
    // リストから先頭オブジェクトをポップ
    auto headWriteLog = m_WriteLogs.takeFirst();

    // !bottomコマンドを書き込むための情報
    THREAD_INFO threadInfo;
    threadInfo.subject  = QString("");                                  // 追加書き込みのため空文字
    threadInfo.from     = m_ThreadInfo.from;                            // 名前欄
    threadInfo.mail     = m_ThreadInfo.mail;                            // メール欄
    threadInfo.message  = QString("!bottom");                           // 書き込む内容
    threadInfo.bbs      = m_ThreadInfo.bbs;                             // BBS名
    threadInfo.key      = headWriteLog.Key;                             // スレッド番号 (スレッドに書き込む場合のみ入力)
    threadInfo.time     = QString::number(WriteMode::getEpocTime());    // 現在時刻をエポックタイムで取得
    threadInfo.shiftjis = m_ThreadInfo.shiftjis;                        // エンコーディング
    threadInfo.expiredXPath = m_ThreadInfo.expiredXPath;                // スレッドのタイトルを抽出するXPath

    Poster poster(this);

    // 掲示板のクッキーを取得
    if (poster.fetchCookies(QUrl(m_WriteInfo.RequestURL))) {
        // クッキーの取得に失敗した場合
        return WRITEERROR::POSTERROR;
    }

    // ログファイルにあるスレッドのURLが生存しているかどうかを確認
    HtmlFetcher fetcher(this);
    auto iExpired = fetcher.checkUrlExistence(QUrl(headWriteLog.Url), headWriteLog.Title, m_WriteInfo.ExpiredXpath, threadInfo.shiftjis);

    if (iExpired == 0) {
        // ログファイルにあるスレッドのURLが生存している場合

        // 書き込むスレッドのレス数が上限に達しているかどうかを確認
        auto ret = checkLastThreadNum();
        if (ret == -1) {
            // 最後尾のレス番号の取得に失敗した場合
            std::cerr << QString("エラー : レス数の取得に失敗").toStdString() << std::endl;
            return WRITEERROR::POSTERROR;
        }
        else if (ret == 1) {
            // 既存のスレッドが最大レス数に達している場合、ログファイルの"thread"オブジェクトのbottomキーを"true"に上書きする
            {
                QMutexLocker logLocker(&m_logMutex);
                if (writeBottomLog(headWriteLog)) {
                    return WRITEERROR::LOGERROR;
                }
            }
        }
        else {
            // 書き込み済みのスレッドが存在する場合

            // 書き込み済みのスレッドに!bottomコマンドを書き込む
            if (poster.PostforWriteThread(QUrl(m_WriteInfo.RequestURL), threadInfo)) {
                // 既存のスレッドへの書き込みに失敗した場合
                return WRITEERROR::POSTERROR;
            }

            // ログファイルの"thread"オブジェクトのbottomキーを"true"に上書きする
            {
                QMutexLocker logLocker(&m_logMutex);
                if (writeBottomLog(headWriteLog)) {
                    return WRITEERROR::LOGERROR;
                }
            }
        }
    }
    else if (iExpired == 1) {
        // ログファイルにあるスレッドのURLが存在しない場合、ログファイルの"thread"オブジェクトのbottomキーを"true"に上書きする
        {
            QMutexLocker logLocker(&m_logMutex);
            if (writeBottomLog(headWriteLog)) {
                return WRITEERROR::LOGERROR;
            }
        }
    }
    else {
        // スレッドの取得に失敗した場合
        return WRITEERROR::POSTERROR;
    }

    return WRITEERROR::SUCCEED;
}


// ログファイル内の該当オブジェクトに対して、"bottom"キーをtrueへ更新
int WriteMode::writeBottomLog(const WRITE_LOG &writeLog)
{
    QFileInfo logFileInfo(m_LogFile);
    QString   lockFilePath = logFileInfo.dir().filePath(logFileInfo.baseName() + ".lock");
    QLockFile lockFile(lockFilePath);

    // 最大30秒の間に、システムは繰り返しロックの取得を試みる
    if (!lockFile.tryLock(30000)) {
        if (lockFile.isLocked())    lockFile.unlock();
        std::cerr << QString("30秒以内にログファイルのロックの取得に失敗").toStdString() << std::endl;

        return -1;
    }

    QFile File(m_LogFile);

    try {
        // ログファイルを開く
        if (!File.open(QIODevice::ReadWrite)) {
            throw std::runtime_error(QString("ログファイルのオープンに失敗  (%1)").arg(m_LogFile).toStdString());
        }

        // ログファイルを読み込む
        QByteArray jsonData = File.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (doc.isNull()) {
            throw std::runtime_error(QString("ログファイルの解析に失敗  (%1)").arg(m_LogFile).toStdString());
        }

        // ログファイルのデータを取得
        QJsonArray jsonArray = doc.array();

        // 同じURLを持つオブジェクトを検索
        bool modified = false;
        for (auto i = 0; i < jsonArray.size(); i++) {
            QJsonObject obj          = jsonArray[i].toObject();
            QJsonObject threadObject = obj["thread"].toObject();
            QString     threadUrl    = threadObject["url"].toString();

            // 書き込みモード 2 または 書き込みモード 3の一般ニュースかどうかを確認
            if (!threadObject["new"].toBool()) {
                // 書き込みモード 1 または 書き込みモード 3の時事ドットコムの速報ニュースの場合
                continue;
            }

            if (threadUrl.compare(writeLog.Url, Qt::CaseSensitive) == 0) {
                // 各オブジェクトの"thread"オブジェクト -> "bottom"キーの値をtrueに更新
                if (!threadObject["bottom"].toBool()) {
                    threadObject["bottom"] = true;
                    obj["thread"]          = threadObject;
                    jsonArray[i]           = obj;
                    modified               = true;
                }
            }
        }

        // 各オブジェクトの値が変更された場合のみログファイルに書き込む
        if (modified) {
            QJsonDocument updatedDoc(jsonArray);
            File.seek(0);
            File.write(updatedDoc.toJson());
            File.resize(File.pos());
        }
    }
    catch (const std::runtime_error &e) {
        if (File.isOpen())          File.close();
        if (lockFile.isLocked())    lockFile.unlock();
        std::cerr << QString("エラー: %1").arg(e.what()).toStdString() << std::endl;

        return -1;
    }
    catch (const std::exception &e) {
        if (File.isOpen())          File.close();
        if (lockFile.isLocked())    lockFile.unlock();
        std::cerr << QString("エラー: %1").arg(e.what()).toStdString() << std::endl;

        return -1;
    }

    // ファイルを閉じる
    File.close();

    // ロックファイルを削除
    lockFile.unlock();

    return 0;
}


// 任意の時間が過ぎた書き込み済みスレッドに対して、レスが無い場合は!bottomコマンドを書き込む
int WriteMode::writeBottomInitialization(const THREAD_INFO &tInfo, const WRITE_INFO &wInfo, const QString &key)
{
    // !bottomコマンドを書き込むための情報
    THREAD_INFO threadInfo;
    threadInfo.subject      = QString("");                                  // 追加書き込みのため空文字
    threadInfo.from         = tInfo.from;                                   // 名前欄
    threadInfo.mail         = tInfo.mail;                                   // メール欄
    threadInfo.message      = QString("!bottom");                           // 書き込む内容
    threadInfo.bbs          = tInfo.bbs;                                    // BBS名
    threadInfo.key          = key;                                          // スレッド番号 (スレッドに書き込む場合のみ入力)
    threadInfo.time         = QString::number(WriteMode::getEpocTime());    // 現在時刻をエポックタイムで取得
    threadInfo.shiftjis     = tInfo.shiftjis;                               // エンコーディング
    threadInfo.expiredXPath = tInfo.expiredXPath;                           // スレッドのタイトルを抽出するXPath

    Poster poster(this);

    // 掲示板のクッキーを取得
    if (poster.fetchCookies(QUrl(wInfo.RequestURL))) {
        // クッキーの取得に失敗した場合
        return WRITEERROR::POSTERROR;
    }

    HtmlFetcher fetcher(this);

    // 指定のスレッドが存在するかどうかを確認
    auto iExpired = fetcher.checkUrlExistence(QUrl(wInfo.ThreadURL), wInfo.ThreadTitle, wInfo.ExpiredXpath, tInfo.shiftjis);

    if (iExpired == 0) {
        // スレッドのURLが生存している場合

        // 該当するスレッドのレス数を取得
        auto num = getLastThreadNum(wInfo);
        if (num == -1) {
            // 最後尾のレス番号の取得に失敗した場合
            std::cerr << QString("エラー : レス数の取得に失敗").toStdString() << std::endl;
            return WRITEERROR::POSTERROR;
        }
        else if (num == 1) {
            // レスが無い場合
            // 書き込み済みのスレッドに!bottomコマンドを書き込む
            if (poster.PostforWriteThread(QUrl(wInfo.RequestURL), threadInfo)) {
                // 既存のスレッドへの書き込みに失敗した場合
                return WRITEERROR::POSTERROR;
            }
        }
        else {
            // レスが存在する場合
            return WRITEERROR::ETC;
        }
    }
    else if (iExpired == 1) {
        // 設定ファイルにあるスレッドのURLが存在しない場合、ログファイルの"thread"オブジェクトのbottomキーを"true"に上書きする
        return WRITEERROR::SUCCEED;
    }
    else {
        // スレッドの取得に失敗した場合
        return WRITEERROR::POSTERROR;
    }

    return WRITEERROR::SUCCEED;
}


// ログファイル内の該当オブジェクトに対して、"bottom"キーをtrueへ更新
int WriteMode::writeBottomLogInitialization(THREAD_INFO tInfo, WRITE_INFO wInfo, int thresholdMilliSec)
{
    QFileInfo logFileInfo(m_LogFile);
    QString   lockFilePath = logFileInfo.dir().filePath(logFileInfo.baseName() + ".lock");
    QLockFile lockFile(lockFilePath);

    // 最大30秒の間に、システムは繰り返しロックの取得を試みる
    if (!lockFile.tryLock(30000)) {
        std::cerr << QString("エラー: 30秒以内に書き込み用ログファイルのロックの取得に失敗").toStdString() << std::endl;
        return -1;
    }

    QFile File(m_LogFile);

    try {
        // ログファイルを開く
        QFile File(m_LogFile);
        if (!File.open(QIODevice::ReadWrite)) {
            throw std::runtime_error(QString("書き込み用ログファイルのオープンに失敗  (%1)").arg(File.errorString()).toStdString());
        }

        // ログファイルの内容を読み込む
        QByteArray jsonData = File.readAll();

        // ログファイルの解析
        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData, &jsonError);

        if (jsonError.error != QJsonParseError::NoError) {
            throw std::runtime_error(jsonError.errorString().toStdString());
        }

        if (!doc.isArray()) {
            throw std::runtime_error("ログファイルのフォーマットが不正です");
        }

        QJsonArray jsonArray        = doc.array();
        QString    currentTime      = getCurrentTime();
        bool       fileModified     = false;

        // 該当するオブジェクトを検索
        for (int i = 0; i < jsonArray.size(); ++i) {
            QJsonObject obj = jsonArray[i].toObject();

            // threadオブジェクトが存在しており、有効なオブジェクトであることを確認
            if (!obj.contains("thread") || !obj["thread"].isObject()) {
                continue;
            }

            QJsonObject threadObj = obj["thread"].toObject();

            // 必要なキーが存在しており、適切な型であることを確認
            if (!threadObj.contains("new")    || !threadObj["new"].isBool()    ||
                !threadObj.contains("bottom") || !threadObj["bottom"].isBool() ||
                !threadObj.contains("time")   || !threadObj["time"].isString()) {
                continue;
            }

            // "thread"オブジェクト -> "new"キーの値がtrue、かつ、"bottom"キーの値がfalseの場合のみ処理を続行
            if (!threadObj["new"].toBool() || threadObj["bottom"].toBool()) {
                continue;
            }

            QString threadTime = threadObj["time"].toString();

            // !bottomコマンドを書き込む指定時間が過ぎているかどうかを確認
            if (!compareTimeStrings(threadTime, currentTime, thresholdMilliSec)) {
                continue;
            }

            // 該当スレッドが存在している場合は、!bottomコマンドを書き込み、ログファイルの"thread"オブジェクト -> "bottom"を"true"に更新
            // 該当スレッドが落ちている場合は、!bottomコマンドを書き込まずに、ログファイルの"thread"オブジェクト -> "bottom"を"true"に更新
            wInfo.ThreadTitle = threadObj["title"].toString();
            wInfo.ThreadURL = threadObj["url"].toString();
            if (writeBottomInitialization(tInfo, wInfo, threadObj["key"].toString())) {
                continue;
            }

            threadObj["bottom"] = true;
            obj["thread"]       = threadObj;
            jsonArray[i]        = obj;
            fileModified        = true;
        }

        if (fileModified) {
            // ファイルの先頭に戻り、更新されたJSONを書き込む
            File.seek(0);
            File.write(QJsonDocument(jsonArray).toJson());
            File.resize(File.pos());
        }
        else {
            std::cout << QString("ログファイル: 起動時に!bottomコマンドが必要なオブジェクトはありません").toStdString() << std::endl;
        }
    }
    catch (const std::runtime_error &e) {
        if (File.isOpen())          File.close();
        if (lockFile.isLocked())    lockFile.unlock();
        std::cerr << QString("エラー: %1").arg(e.what()).toStdString() << std::endl;

        return -1;
    }
    catch (const std::exception &e) {
        if (File.isOpen())  File.close();
        if (lockFile.isLocked())    lockFile.unlock();
        std::cerr << QString("エラー: %1").arg(e.what()).toStdString() << std::endl;

        return -1;
    }

    File.close();

    lockFile.unlock();

    return 0;
}


// 指定された時間を超えているかどうかを確認
bool WriteMode::compareTimeStrings(const QString &time1, const QString &time2, int thresholdMilliSec)
{
    // 日時文字列のフォーマットを指定
    QString format = "yyyy年M月d日 h時m分";

    // QStringをQDateTimeに変換
    QDateTime dt1 = QDateTime::fromString(time1, format);
    QDateTime dt2 = QDateTime::fromString(time2, format);

    // 2つの日時の差をミリ秒単位で計算
    qint64 msDiff = dt1.msecsTo(dt2);

    // 指定された時間を超えているかどうかを確認
    if (qAbs(msDiff) < thresholdMilliSec) {
        // 指定された時間を超えていない場合
        return false;
    }

    // 指定された時間を超えている場合
    return true;
}
