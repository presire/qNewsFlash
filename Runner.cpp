#include <QCoreApplication>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimeZone>
#include <QXmlStreamReader>
#include <QTextDocumentFragment>
#include <QException>
#include <iostream>
#include <random>
#include <utility>
#include <set>
#include "Runner.h"
#include "HtmlFetcher.h"
#include "RandomGenerator.h"


#ifdef Q_OS_LINUX
Runner::Runner(QStringList _args, QString user, QObject *parent) : m_args(std::move(_args)), m_User(std::move(user)), m_SysConfFile(""), m_interval(30 * 60 * 1000),
    m_pNotifier(std::make_unique<QSocketNotifier>(fileno(stdin), QSocketNotifier::Read, this)), m_stopRequested(false),
    manager(std::make_unique<QNetworkAccessManager>(this)),
    QObject{parent}
{
    connect(m_pNotifier.get(), &QSocketNotifier::activated, this, &Runner::onReadyRead);        // キーボードシーケンスの有効化
}
#elif Q_OS_WIN
Runner::Runner(QStringList _args, QObject *parent) : m_args(std::move(_args)), m_SysConfFile(""), m_interval(30 * 60 * 1000),
    m_pNotifier(std::make_unique<QWinEventNotifier>(fileno(stdin), QWinEventNotifier::Read, this)), m_stopRequested(false),
    manager(std::make_unique<QNetworkAccessManager>(this)),
    QObject{parent}
{
    connect(m_pNotifier.get(), &QWinEventNotifier::activated, this, &Runner::onReadyRead);      // キーボードシーケンスの有効化
}
#endif



void Runner::run()
{
    // メイン処理

    // コマンドラインオプションの確認
    // 設定ファイルおよびバージョン情報
    m_args.removeFirst();   // プログラムのパスを削除
    for (auto &arg : m_args) {
        if(arg.mid(0, 10) == "--sysconf=") {
            // 設定ファイルのパス
            auto option = arg;
            option.replace("--sysconf=", "", Qt::CaseSensitive);

            // 先頭と末尾にクォーテーションが存在する場合は取り除く
            if ((option.startsWith('\"') && option.endsWith('\"')) || (option.startsWith('\'') && option.endsWith('\''))) {
                option = option.mid(1, option.length() - 2);
            }

            if (getConfiguration(option)) {
                QCoreApplication::exit();
                return;
            }
            else {
                m_SysConfFile = option;
            }

            break;
        }
        else if (m_args.length() == 1 && (arg.compare("--version", Qt::CaseSensitive) == 0 || arg.compare("-v", Qt::CaseInsensitive) == 0) ) {
            // バージョン情報
            auto version = QString("qNewsFlash %1.%2.%3\n").arg(PROJECT_VERSION_MAJOR).arg(PROJECT_VERSION_MINOR).arg(PROJECT_VERSION_PATCH)
                           + QString("ライセンス : UNLICENSE\n")
                           + QString("ライセンスの詳細な情報は、<http://unlicense.org/> を参照してください\n\n")
                           + QString("開発者 : presire with ﾘ* ﾞㇷﾞ)ﾚ の みんな\n\n");
            std::cout << version.toStdString() << std::endl;

            QCoreApplication::exit();
            return;
        }
        else if (m_args.length() == 1 && (arg.compare("--help", Qt::CaseSensitive) == 0 || arg.compare("-h", Qt::CaseSensitive) == 0) ) {
            // ヘルプ
            auto help = QString("使用法 : qNewsFlash [オプション]\n\n")
                           + QString("  --sysconf=<qNewsFlash.jsonファイルのパス>\t\t設定ファイルのパスを指定する\n")
                           + QString("  -v, -V, --version                      \t\tバージョン情報を表示する\n\n");
            std::cout << help.toStdString() << std::endl;

            QCoreApplication::exit();
            return;
        }
        else {
            std::cerr << QString("エラー : 不明なオプションです - %1").arg(arg).toStdString() << std::endl;

            QCoreApplication::exit();
            return;
        }
    }

    if (m_SysConfFile.isEmpty()) {
        std::cerr << QString("エラー : 設定ファイルのパスが不明です").toStdString() << std::endl;

        QCoreApplication::exit();
        return;
    }

    // ログファイルの設定
    if (setLogFile(m_LogFile)) {
        QCoreApplication::exit();
        return;
    }

    // ログファイルから、昨日以前(昨日も含む)の書き込み済みのニュース記事を削除
    if (deleteLogNotToday()) {
        QCoreApplication::exit();
        return;
    }

    // ログファイルから、今日と昨日の書き込み済みのニュース記事を取得
    // また、取得した記事群のデータは、メンバ変数m_WrittenArticlesに保存
    if (getDatafromWrittenLog()) {
        QCoreApplication::exit();
        return;
    }

    // News APIを有効にしている場合、かつ、News APIのキーが空の場合はエラー
    if (m_bNewsAPI && m_API.isEmpty()) {
        std::cerr << QString("エラー : News APIが有効化されていますが、News APIのキーが設定されていません").toStdString() << std::endl;

        QCoreApplication::exit();
        return;
    }

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
    // JSONファイル(スレッド書き込み用)のパスが空の場合は、デフォルトのパスを使用
    if (m_WriteFile.isEmpty()) {
        m_WriteFile = QString("/tmp/qNewsFlashWrite.json");
    }
#endif

    if (m_AutoFetch) {
        // ニュース記事の自動取得タイマの開始
        connect(&m_timer, &QTimer::timeout, this, &Runner::fetch);
        m_timer.start(static_cast<int>(m_interval));

        // 速報記事の自動取得タイマの開始
        connect(&m_JiJiTimer, &QTimer::timeout, this, &Runner::JiJiFlashfetch);
        m_JiJiTimer.start(static_cast<int>(m_JiJiinterval));
    }

    // 本ソフトウェア開始直後に各ニュース記事を読み込む場合は、コメントを解除して、fetch()メソッドを実行する
    // コメントアウトしている場合、最初に各ニュース記事を読み込むタイミングは、タイマの指定時間後となる
    fetch();

    // 本ソフトウェア開始直後に時事ドットコムから速報記事を読み込む場合は、コメントを解除して、JiJiFlashfetch()メソッドを実行する
    // コメントアウトしている場合、最初に速報記事を読み込むタイミングは、タイマの指定時間後となる
    JiJiFlashfetch();

    // ソフトウェアの自動起動が無効の場合
    // Cronを使用する場合、または、ワンショットで動作させる場合の処理
    if (!m_AutoFetch) {
        // 既に[q]キーまたは[Q]キーが押下されている場合は再度終了処理を行わない
        if (!m_stopRequested.load()) {
            // ソフトウェアを終了する
            QCoreApplication::exit();
            return;
        }
    }
}


void Runner::fetch()
{
    // 現在の日時を取得して日付が変わっているかどうかを確認
    /// 日本のタイムゾーンを設定
    auto now = QDateTime::currentDateTimeUtc().toTimeZone(QTimeZone("Asia/Tokyo"));

    /// 現在の年月日のみを取得
    auto currentDate = now.date();
    auto date = QString("%1/%2/%3").arg(currentDate.year()).arg(currentDate.month()).arg(currentDate.day());

    /// 前回のニュース記事を取得した日付と比較
    if (m_LastUpdate.compare(date, Qt::CaseSensitive) != 0) {
        /// 日付が変わっている場合
        m_LastUpdate = date;

        /// メンバ変数m_WrittenArticlesから、書き込み済みの2日以上前の記事群を削除
        m_WrittenArticles.clear();

        /// ログファイルから、2日以上前の書き込み済みニュース記事を削除
        if (deleteLogNotToday()) {
            QCoreApplication::exit();
            return;
        }

        /// ログファイルから、今日と昨日の書き込み済みのニュース記事を取得
        /// また、取得した記事群のデータはメンバ変数m_WrittenArticlesに格納
        if (getDatafromWrittenLog()) {
            QCoreApplication::exit();
            return;
        }
    }

    // 設定ファイルの"update"キーを更新
    if (updateDateJson(m_LastUpdate)) {
        QCoreApplication::exit();
        return;
    }

    // 前回取得した書き込み前の記事群(選定前)を初期化
    m_BeforeWritingArticles.clear();

    if (m_stopRequested.load()) return;

    // News APIの日本国内の記事を取得
    // ただし、無料版のNews APIの記事は24時間遅れであるため、News APIを使用する場合は有料版を推奨する
    if (m_bNewsAPI) {
        QUrl url(m_NewsAPIRSS.append(m_API));

        /// HTTPリクエストを作成して、ヘッダを設定
        QNetworkRequest request(url);

        /// HTTPリクエストを送信
        m_pReply = manager->get(request);

        /// HTTPレスポンスを受信した後、Runner::fetchNewsAPI()メソッドを実行
        QObject::connect(m_pReply, &QNetworkReply::finished, this, &Runner::fetchNewsAPI);

        /// レスポンス待機
        QEventLoop loop;
        QObject::connect(this, &Runner::NewAPIfinished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されているかどうかを確認
    if (m_stopRequested.load()) return;

    // 時事ドットコムの記事を取得
    if (m_bJiJi) {
        // 時事ドットコムのRSSフィードのURLを指定
        QUrl urlJiJi(m_JiJiRSS);

        /// HTTPリクエストを作成して、ヘッダを設定
        QNetworkRequest requestJiJi(urlJiJi);

        /// HTTPリクエストを送信
        m_pReplyJiJi = manager->get(requestJiJi);

        /// HTTPレスポンスを受信した後、Runner::fetchJiJiRSS()メソッドを実行
        QObject::connect(m_pReplyJiJi, &QNetworkReply::finished, this, &Runner::fetchJiJiRSS);

        /// レスポンス待機
        QEventLoop loop;
        QObject::connect(this, &Runner::JiJifinished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されているかどうかを確認
    if (m_stopRequested.load()) return;

    // 共同通信の記事を取得
    if (m_bKyodo) {
        // 共同通信のRSSフィードのURLを指定
        QUrl urlKyodo(m_KyodoRSS);

        /// HTTPリクエストを作成して、ヘッダを設定
        QNetworkRequest requestKyodo(urlKyodo);

        /// HTTPリクエストを送信
        m_pReplyKyodo = manager->get(requestKyodo);

        /// HTTPレスポンスを受信した後、Runner::fetchJiJiRSS()メソッドを実行
        QObject::connect(m_pReplyKyodo, &QNetworkReply::finished, this, &Runner::fetchKyodoRSS);

        /// レスポンス待機
        QEventLoop loop;
        QObject::connect(this, &Runner::Kyodofinished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;

    // 朝日新聞デジタルの記事を取得
    if (m_bAsahi) {
        // 朝日新聞デジタルのRSSフィードのURLを指定
        QUrl urlAsahi(m_AsahiRSS);

        /// HTTPリクエストを作成して、ヘッダを設定
        QNetworkRequest requestAsahi(urlAsahi);

        /// HTTPリクエストを送信
        m_pReplyAsahi = manager->get(requestAsahi);

        /// HTTPレスポンスを受信した後、Runner::fetchCNetRSS()メソッドを実行
        QObject::connect(m_pReplyAsahi, &QNetworkReply::finished, this, &Runner::fetchAsahiRSS);

        /// レスポンス待機
        QEventLoop loop;
        QObject::connect(this, &Runner::Asahifinished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;

    // CNET Japanの記事を取得
    if (m_bCNet) {
        // CNET JapanのRSSフィードのURLを指定
        QUrl urlCNet(m_CNETRSS);

        /// HTTPリクエストを作成して、ヘッダを設定
        QNetworkRequest requestCNet(urlCNet);

        /// HTTPリクエストを送信
        m_pReplyCNet = manager->get(requestCNet);

        /// HTTPレスポンスを受信した後、Runner::fetchCNetRSS()メソッドを実行
        QObject::connect(m_pReplyCNet, &QNetworkReply::finished, this, &Runner::fetchCNetRSS);

        /// レスポンス待機
        QEventLoop loop;
        QObject::connect(this, &Runner::CNetfinished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;

    // ハンギョレジャパンの記事を取得
    if (m_bHanJ) {
        // ハンギョレジャパンのRSSフィードのURLを指定
        QUrl urlHanJ(m_HanJRSS);

        /// HTTPリクエストを作成して、ヘッダを設定
        QNetworkRequest requestHanJ(urlHanJ);

        /// HTTPリクエストを送信
        m_pReplyHanJ = manager->get(requestHanJ);

        /// HTTPレスポンスを受信した後、Runner::fetchJiJiRSS()メソッドを実行
        QObject::connect(m_pReplyHanJ, &QNetworkReply::finished, this, &Runner::fetchHanJRSS);

        /// レスポンス待機
        QEventLoop loop;
        QObject::connect(this, &Runner::HanJfinished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;

    // ロイター通信の記事を取得
    if (m_bReuters) {
        // ロイター通信のRSSフィードのURLを指定
        QUrl urlReuters(m_ReutersRSS);

        /// HTTPリクエストを作成して、ヘッダを設定
        QNetworkRequest requestReuters(urlReuters);

        /// HTTPリクエストを送信
        m_pReplyReuters = manager->get(requestReuters);

        /// HTTPレスポンスを受信した後、Runner::fetchJiJiRSS()メソッドを実行
        QObject::connect(m_pReplyReuters, &QNetworkReply::finished, this, &Runner::fetchReutersRSS);

        /// レスポンス待機
        QEventLoop loop;
        QObject::connect(this, &Runner::Reutersfinished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;

    // 東京新聞の記事を取得
    if (m_bTokyoNP) {
        fetchTokyoNP();
    }

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;

    // 取得したニュース記事群を操作
    if (!m_BeforeWritingArticles.empty()) {
        // 取得したニュース記事群が存在する場合

        // ニュース記事が複数存在する場合、ランダムで決定する (乱数生成により配列のインデックスを決める)
        /// 乱数生成の方法
        /// x86 / x64版 : CPUのタイムスタンプカウンタ(TSC)の値をハッシュ化したものをXorshiftする (それをシード値とする)
        ///               メルセンヌ・ツイスターにそのシード値を入力して、一様分布したものをインデックス値としている
        /// ARM / AArch64版 : /dev/urandom (CSPRNG) の値をハッシュ化したものをXorshiftする (それをシード値とする)
        ///                   もし、/dev/urandomの値の取得に失敗した場合は、std::chrono::high_resolution_clockを使用して現在の精密な時刻を取得する
        ///                   メルセンヌ・ツイスターにそのシード値を入力して、一様分布したものをインデックス値としている
        ///
        /// 備考 : ARM / AArch64において、CPUのタイムスタンプカウンタ(TSC)と似た値 (CNTPCTレジスタの値) を取得することもできるが、
        ///       一部のARM / AArch64ベースのデバイスでは、セキュリティや安定性の観点からユーザモードのプロセスが低レベルのハードウェアリソースにアクセスすることを制限しており、
        ///       特権モード (カーネルモード) での実行が必要になる場合がある
        ///       また、CNTPCTレジスタにアクセスするライブラリのライセンスがGPLであるため現在は使用していない
        auto article = selectArticle();

        // ニュース記事のタイトル --> 公開日 --> 本文の一部 --> URL の順に並べて書き込む
        // ただし、ニュース記事の本文を取得しない場合は、ニュース記事のタイトル --> 公開日 --> URL の順とする
        auto [title, paragraph, link, pubDate] = article.getArticleData();

        m_ThreadInfo.message = QString("%1%2%3%4").arg(title + "\n",
                                                       pubDate + "\n\n",
                                                       paragraph.length() == 0 ? "" : paragraph + "\n",
                                                       link);

        // 現在時刻をエポックタイムで取得
        auto epocTime = GetEpocTime();
        m_ThreadInfo.time = QString::number(epocTime);

        Poster poster(this);

        // 掲示板のクッキーを取得
        if (poster.fetchCookies(QUrl(m_RequestURL))) {
            // クッキーの取得に失敗した場合
            return;
        }

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR >= 1) && (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR <= 2)
        // qNewsFlash 0.1.0から0.2.xまでの機能

        // スレッドのタイトルを変更するかどうか (防弾嫌儲およびニュース速報(Libre)等の掲示板で使用可能)
        if (m_changeTitle) m_ThreadInfo.message.prepend("!chtt");

        // 既存のスレッドに書き込み
        if (poster.PostforWriteThread(QUrl(m_RequestURL), m_ThreadInfo)) {
            // 既存のスレッドへの書き込みに失敗した場合
            return;
        }
#elif QNEWSFLASH_VERSION_MAJOR > 0 || (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR >= 3)
        // qNewsFlash 0.3.0以降の機能

        // 指定のスレッドがレス数の上限に達して書き込めない場合、スレッドを新規作成する
        HtmlFetcher fetcher(this);
        //auto iExpired = fetcher.checkUrlExistence(QUrl(m_ThreadURL), m_ExpiredElement, m_ExpiredXpath, m_ThreadInfo.shiftjis);
        auto iExpired = fetcher.checkUrlExistence(QUrl(m_ThreadURL), m_ThreadTitle, m_ExpiredXpath, m_ThreadInfo.shiftjis);
        if (iExpired == 0) {
            // 設定ファイルにあるスレッドのURLが生存している場合

            // ニュース記事を書き込むスレッドのレス数を取得
            auto ret = checkLastThreadNum();
            if (ret == -1) {
                // 最後尾のレス番号の取得に失敗した場合
                std::cerr << QString("エラー : レス数の取得に失敗").toStdString() << std::endl;
                return;
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

                if (poster.PostforCreateThread(QUrl(m_RequestURL), m_ThreadInfo)) {
                    // スレッドの新規作成に失敗した場合
                    return;
                }

                // 新規作成したスレッドのURLとスレッド番号を取得
                m_ThreadURL      = poster.GetNewThreadURL();
                m_ThreadInfo.key = poster.GetNewThreadNum();

                // スレッド情報 (スレッドのURLおよびスレッド番号) を設定ファイルに保存
                if (UpdateThreadJson(m_ThreadInfo.subject)) {
                    // スレッド情報の保存に失敗
                    return;
                }
            }
            else {
                // 既存のスレッドが存在する場合

                // スレッドのタイトルを変更するかどうか (防弾嫌儲およびニュース速報(Libre)等の掲示板で使用可能)
                if (m_changeTitle) m_ThreadInfo.message.prepend("!chtt");

                // 既存のスレッドに書き込み
                if (poster.PostforWriteThread(QUrl(m_RequestURL), m_ThreadInfo)) {
                    // 既存のスレッドへの書き込みに失敗した場合
                    return;
                }

                // スレッドのタイトルが正常に変更されているかどうかを確認
                if (m_changeTitle && !CompareThreadTitle(QUrl(m_ThreadURL), title)) {
                    // スレッドのタイトルが正常に変更された場合
                    // スレッド情報 (スレッドのタイトル、スレッドのURL、スレッド番号) を設定ファイルに保存
                    if (UpdateThreadJson(title)) {
                        // スレッド情報の保存に失敗
                        return;
                    }
                }
                else {
                    // スレッドのタイトルが変更されなかった場合
                    // スレッド情報 (スレッドのURLおよびスレッド番号) を設定ファイルに保存
                    if (UpdateThreadJson(m_ThreadTitle)) {
                        // スレッド情報の保存に失敗
                        return;
                    }
                }
            }
        }
        else if (iExpired == 1) {
            // 設定ファイルにあるスレッドのURLが存在しない、または、落ちている場合 (スレッドを新規作成する)

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
            if (poster.PostforCreateThread(QUrl(m_RequestURL), m_ThreadInfo)) {
                // スレッドの新規作成に失敗した場合
                return;
            }

            // 新規作成したスレッドのURLとスレッド番号を取得
            m_ThreadURL      = poster.GetNewThreadURL();
            m_ThreadInfo.key = poster.GetNewThreadNum();

            // スレッド情報 (スレッドのURLおよびスレッド番号) を設定ファイルに保存
            if (UpdateThreadJson(m_ThreadInfo.subject)) {
                // スレッド情報の保存に失敗
                return;
            }
        }
        else {
            return;
        }

#endif

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
        // qNewsFlash 0.1.0未満の機能
        // JSONファイル(スレッド書き込み用)に書き込む
        // 該当スレッドへの書き込み処理は各自で実装する
        if (writeJSON(article)) {
            QCoreApplication::exit();
            return;
        }
#endif

        // 書き込み済みの記事を履歴として登録 (同じ記事を1日に2回以上書き込まないようにする)
        // ただし、2日前以上の書き込み済み記事の履歴は削除する
        m_WrittenArticles.append(article);

        // ログファイルに書き込む
        if (writeLog(article)) {
            QCoreApplication::exit();
            return;
        }
    }
#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
    // qNewsFlash 0.1.0未満の機能
    else {

        // スレッド書き込み用のJSONファイルの内容を空にする
        if (truncateJSON()) {
            QCoreApplication::exit();
            return;
        }
    }
#endif

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;
}


void Runner::fetchNewsAPI()
{
    if (m_pReply->error() == QNetworkReply::NoError) {
        // 正常にレスポンスを取得した場合
        QByteArray    response  = m_pReply->readAll();
        QJsonDocument jsonDoc   = QJsonDocument::fromJson(response);
        QJsonObject   jsonObj   = jsonDoc.object();
        QJsonArray    articles  = jsonObj["articles"].toArray();

        for (auto i = 0; i < articles.count(); i++) {
            QJsonObject article = articles[i].toObject();

            // 特定メディアの記事を排除
            /// sourceオブジェクトの取得
            QJsonObject sourceObject = article["source"].toObject();

            /// idキーの値の取得
            QString sourceName = sourceObject["name"].toString();

            if (m_ExcludeMedia.contains(sourceName)) continue;

            // UTC時刻から日本時間へ変換
            auto utcDate = article["publishedAt"].toString();
            auto convDate = Runner::convertJPDate(utcDate);

            // ニュースの公開日を確認
            // 今日のニュース記事ではない場合、または、指定時間以内のニュース記事ではない場合は無視
            auto isCheckDate = m_WithinHours == 0 ? isToday(convDate) : isHoursAgo(convDate);
            if (!isCheckDate) {
                continue;
            }

            // 書き込み済みの記事が存在する場合は無視
            // 書き込み済みの記事かどうかを判断する方法として、同一のURLかどうかを確認している
            bool bWritten = false;
            for (auto &writtenArticle : m_WrittenArticles) {
                QString url = "";
                std::tie(std::ignore, std::ignore, url, std::ignore) = writtenArticle.getArticleData();

                if (url.compare(article["url"].toString(), Qt::CaseSensitive)) {
                    bWritten = true;
                    break;
                }
            }
            if (bWritten) continue;

            // 本文が指定文字数以上の場合、指定文字数のみを抽出
            auto paragraph = article["description"].toString();
            paragraph = paragraph.size() > m_MaxParagraph ? paragraph.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : paragraph;

            // 書き込む前の記事群
            Article articleObj(article["title"].toString(), paragraph, article["url"].toString(), convDate);
            m_BeforeWritingArticles.append(articleObj);

#ifdef _DEBUG
            qDebug() << "Title : " << article["title"].toString();
            qDebug() << "Paragraph : " << paragraph;
            qDebug() << "URL : " << article["url"].toString();
            qDebug() << "Date : " << convDate;
            qDebug() << "";
#endif
        }
    }
    else {
        // レスポンスの取得に失敗した場合
        std::cerr << m_pReply->errorString().toStdString() << std::endl;
    }

    m_pReply->deleteLater();

    emit NewAPIfinished();
}


void Runner::fetchJiJiRSS()
{
    auto byteArray  = m_pReplyJiJi->readAll();
    auto xmlContent = byteArray.constData();

    // libxml2の初期化
    xmlInitParser();
    LIBXML_TEST_VERSION

    // メモリバッファからXMLをパース
    auto *doc = xmlReadMemory(xmlContent, byteArray.size(), "noname.xml", nullptr, 0);
    if (doc == nullptr) {
        std::cerr << "Failed to parse XML from memory" << std::endl;
        emit JiJifinished();

        return;
    }

    // ルート要素を取得
    auto *root_element = xmlDocGetRootElement(doc);

    // 各itemタグを処理
    itemTagsforJiJi(root_element);

    // ドキュメントを解放
    xmlFreeDoc(doc);

    // libxml2をクリーンアップ
    xmlCleanupParser();

    m_pReplyJiJi->deleteLater();

    emit JiJifinished();
}


void Runner::itemTagsforJiJi(xmlNode *a_node)
{
    for (auto cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST "item") == 0) {
            xmlNode *itemChild = cur_node->children;
            QString title       = "",
                    paragraph   = "",
                    link        = "",
                    date        = "";
            bool    bSkipNews   = false;

            while (itemChild) {
                if (itemChild->type == XML_ELEMENT_NODE) {
                    if (xmlStrcmp(itemChild->name, BAD_CAST "title") == 0) {
                        title = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "link") == 0) {
                        link = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // 記事の本文から指定文字数分のみ取得
                        QUrl url(link);
                        HtmlFetcher fetcher(m_MaxParagraph, this);

                        if (fetcher.fetch(url, true)) {
                            // 本文の取得に失敗した場合
                            bSkipNews = true;
                            break;
                        }

                        paragraph = fetcher.getParagraph();

                        // URLのクエリ部分を操作
                        QUrlQuery query(url);
                        auto convURL = url.adjusted(QUrl::RemoveQuery).toString() + QString("?k=") + query.queryItemValue("k");

                        link = convURL;
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "date") == 0) {
                        date = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // 日付のフォーマットをISO 8601形式("yyyy-MM-ddThh:mm:ssZ")から"yyyy年M月d日 h時m分"へ変更
                        date = convertDate(date);

                        // ニュースの公開日を確認
                        auto isCheckDate = m_WithinHours == 0 ? isToday(date) : isHoursAgo(date);
                        if (!isCheckDate) {
                            bSkipNews = true;
                            break;
                        }
                    }
                }
                itemChild = itemChild->next;
            }

            // 今日のニュース記事ではない場合、または、指定時間以内のニュース記事ではない場合は無視
            if (bSkipNews) continue;

            // 既に書き込み済みの記事の場合は無視
            bool bWritten = false;
            for (auto &writtenArticle : m_WrittenArticles) {
                QString url = "";
                std::tie(std::ignore, std::ignore, url, std::ignore) = writtenArticle.getArticleData();

                if (url.compare(link, Qt::CaseSensitive) == 0) {
                    bWritten = true;
                    break;
                }
            }
            if (bWritten) continue;

            // 書き込む前の記事群
            Article article(title, paragraph, link, date);
            m_BeforeWritingArticles.append(article);

#ifdef _DEBUG
            qDebug() << "Title : " << title;
            qDebug() << "Paragraph : " << paragraph;
            qDebug() << "URL : " << link;
            qDebug() << "Date : " << date;
            qDebug() << "";
#endif
        }
        itemTagsforJiJi(cur_node->children);
    }
}


void Runner::fetchKyodoRSS()
{
    auto byteArray  = m_pReplyKyodo->readAll();
    auto xmlContent = byteArray.constData();

    // libxml2の初期化
    xmlInitParser();
    LIBXML_TEST_VERSION

    // メモリバッファからXMLをパース
    auto *doc = xmlReadMemory(xmlContent, byteArray.size(), "noname.xml", nullptr, 0);
    if (doc == nullptr) {
        std::cerr << "Failed to parse XML from memory" << std::endl;
        emit Kyodofinished();

        return;
    }

    // ルート要素を取得
    auto *root_element = xmlDocGetRootElement(doc);

    // 各itemタグを処理
    itemTagsforKyodo(root_element);

    // ドキュメントを解放
    xmlFreeDoc(doc);

    // libxml2をクリーンアップ
    xmlCleanupParser();

    m_pReplyKyodo->deleteLater();

    emit Kyodofinished();
}


void Runner::itemTagsforKyodo(xmlNode *a_node)
{
    for (auto cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST "item") == 0) {
            xmlNode *itemChild = cur_node->children;
            QString title       = "",
                    paragraph   = "",
                    link        = "",
                    date        = "";
            bool    bSkipNews   = false;

            while (itemChild) {
                if (itemChild->type == XML_ELEMENT_NODE) {
                    if (xmlStrcmp(itemChild->name, BAD_CAST "title") == 0) {
                        title = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "description") == 0) {
                        paragraph = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // 本文の先頭にあるyyyy年M月d日=<文字数> (全角数字と全角カンマを含む) を削除
                        static QRegularExpression re2("^[0-9０-９]{4}年[0-9０-９]{1,2}月[0-9０-９]{1,2}日[=＝][0-9０-９,，]+");
                        paragraph.remove(re2);

                        // 不要な文字を削除 (スペース等)
                        static QRegularExpression re1("[\\s]", QRegularExpression::CaseInsensitiveOption);
                        paragraph = paragraph.replace(re1, "").replace(" ", "").replace(" ", "").replace("\u3000", "");

                        // 先頭に"＊"がある場合は削除
                        if (paragraph.startsWith("＊")) {
                            paragraph.remove(0, 1);
                        }

                        // "&#8230;"がある場合は削除
                        paragraph.replace("&#8230;", "");

                        // QTextDocumentFragment::fromHtml()を使用して変換
                        paragraph = QTextDocumentFragment::fromHtml(paragraph).toPlainText();

                        // 本文が指定文字数以上の場合、指定文字数分のみを抽出
                        paragraph = paragraph.size() > m_MaxParagraph ? paragraph.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : paragraph;
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "link") == 0) {
                        link = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        if (m_NewsOnly) {
                            // ニュース記事の枠ではない場合は該当記事を無視
                            if (!link.startsWith("https://www.kyodo.co.jp/news/")) {
                                bSkipNews = true;
                                break;
                            }
                        }
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "pubDate") == 0) {
                        date = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // 日付のフォーマットをUTCから日本時間の"yyyy年M月d日 h時m分"形式へ変更
                        date = convertJPDateforKyodo(date);

                        // ニュースの公開日を確認
                        auto isCheckDate = m_WithinHours == 0 ? isToday(date) : isHoursAgo(date);
                        if (!isCheckDate) {
                            bSkipNews = true;
                            break;
                        }
                    }
                }
                itemChild = itemChild->next;
            }

            // 今日のニュース記事ではない場合、または、指定時間以内のニュース記事ではない場合は無視
            if (bSkipNews) continue;

            // 既に書き込み済みの記事の場合は無視
            bool bWritten = false;
            for (auto &writtenArticle : m_WrittenArticles) {
                QString url = "";
                std::tie(std::ignore, std::ignore, url, std::ignore) = writtenArticle.getArticleData();

                if (url.compare(link, Qt::CaseSensitive) == 0) {
                    bWritten = true;
                    break;
                }
            }
            if (bWritten) continue;

            // 書き込む前の記事群
            Article article(title, paragraph, link, date);
            m_BeforeWritingArticles.append(article);

#ifdef _DEBUG
            qDebug() << "Title : " << title;
            qDebug() << "Paragraph : " << paragraph;
            qDebug() << "URL : " << link;
            qDebug() << "Date : " << date;
            qDebug() << "";
#endif
        }
        itemTagsforKyodo(cur_node->children);
    }
}


void Runner::fetchAsahiRSS()
{
    auto byteArray  = m_pReplyAsahi->readAll();
    auto xmlContent = byteArray.constData();

    // libxml2の初期化
    xmlInitParser();
    LIBXML_TEST_VERSION

    // メモリバッファからXMLをパース
    auto *doc = xmlReadMemory(xmlContent, byteArray.size(), "noname.xml", nullptr, 0);
    if (doc == nullptr) {
        std::cerr << "Failed to parse XML from memory" << std::endl;
        emit Asahifinished();

        return;
    }

    // ルート要素を取得
    auto *root_element = xmlDocGetRootElement(doc);

    // 各itemタグを処理
    itemTagsforAsahi(root_element);

    // ドキュメントを解放
    xmlFreeDoc(doc);

    // libxml2をクリーンアップ
    xmlCleanupParser();

    m_pReplyAsahi->deleteLater();

    emit Asahifinished();
}


void Runner::itemTagsforAsahi(xmlNode *a_node)
{
    for (auto cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST "item") == 0) {
            xmlNode *itemChild = cur_node->children;
            QString title       = "",
                    paragraph   = "",
                    link        = "",
                    date        = "";
            bool    bSkipNews   = false;

            while (itemChild) {
                if (itemChild->type == XML_ELEMENT_NODE) {
                    if (xmlStrcmp(itemChild->name, BAD_CAST "title") == 0) {
                        title = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "link") == 0) {
                        link = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // refクエリパラメータを削除
                        QUrl url(link);
                        QUrlQuery query(url.query());

                        query.removeQueryItem("ref");
                        url.setQuery(query);

                        link = url.toString();

                        // 記事の本文から指定文字数分のみ取得
                        HtmlFetcher fetcher(m_MaxParagraph, this);

                        if (fetcher.fetch(link, true)) {
                            // 本文の取得に失敗した場合
                            bSkipNews = true;
                            break;
                        }

                        paragraph = fetcher.getParagraph();
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "date") == 0) {
                        date = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // 日付のフォーマットを"yyyy-MM-ddThh:mm+09:00"から"yyyy年M月d日 h時m分"へ変更
                        date = convertDate(date);

                        // ニュースの公開日を確認
                        auto isCheckDate = m_WithinHours == 0 ? isToday(date) : isHoursAgo(date);
                        if (!isCheckDate) {
                            bSkipNews = true;
                            break;
                        }
                    }
                }
                itemChild = itemChild->next;
            }

            // 今日のニュース記事ではない場合、または、指定時間以内のニュース記事ではない場合は無視
            if (bSkipNews) continue;

            // 既に書き込み済みの記事の場合は無視
            bool bWritten = false;
            for (auto &writtenArticle : m_WrittenArticles) {
                QString url = "";
                std::tie(std::ignore, std::ignore, url, std::ignore) = writtenArticle.getArticleData();

                if (url.compare(link, Qt::CaseSensitive) == 0) {
                    bWritten = true;
                    break;
                }
            }
            if (bWritten) continue;

            // 書き込む前の記事群
            Article article(title, paragraph, link, date);
            m_BeforeWritingArticles.append(article);

#ifdef _DEBUG
            qDebug() << "Title : " << title;
            qDebug() << "Paragraph : " << paragraph;
            qDebug() << "URL : " << link;
            qDebug() << "Date : " << date;
            qDebug() << "";
#endif
        }
        itemTagsforAsahi(cur_node->children);
    }
}


void Runner::fetchCNetRSS()
{
    auto byteArray  = m_pReplyCNet->readAll();
    auto xmlContent = byteArray.constData();

    // libxml2の初期化
    xmlInitParser();
    LIBXML_TEST_VERSION

    // メモリバッファからXMLをパース
    auto *doc = xmlReadMemory(xmlContent, byteArray.size(), "noname.xml", nullptr, 0);
    if (doc == nullptr) {
        std::cerr << "Failed to parse XML from memory" << std::endl;
        emit CNetfinished();

        return;
    }

    // ルート要素を取得
    auto *root_element = xmlDocGetRootElement(doc);

    // 各itemタグを処理
    itemTagsforCNet(root_element);

    // ドキュメントを解放
    xmlFreeDoc(doc);

    // libxml2をクリーンアップ
    xmlCleanupParser();

    m_pReplyCNet->deleteLater();

    emit CNetfinished();
}


void Runner::itemTagsforCNet(xmlNode *a_node)
{
    for (auto cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST "item") == 0) {
            xmlNode *itemChild = cur_node->children;
            QString title       = "",
                    paragraph   = "",
                    link        = "",
                    date        = "";
            bool    bSkipNews   = false;

            while (itemChild) {
                if (itemChild->type == XML_ELEMENT_NODE) {
                    if (xmlStrcmp(itemChild->name, BAD_CAST "title") == 0) {
                        title = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "description") == 0) {
                        // 現在、RSSからニュース記事の概要を取得しない
                        // 該当するニュース記事のURLにアクセスして、記事の概要を抽出する
//                        paragraph = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

//                        // 不要なhtmlタグを除去
//                        static QRegularExpression re2("<br .*</a>", QRegularExpression::DotMatchesEverythingOption);
//                        paragraph.remove(re2);

//                        // 不要な文字を削除 (\n, \t)
//                        static QRegularExpression re1("[\t\n]", QRegularExpression::CaseInsensitiveOption);
//                        paragraph = paragraph.replace(re1, "");

//                        // 不要な文字を削除 (スペース等)
//                        static QRegularExpression re3("[\\s]", QRegularExpression::CaseInsensitiveOption);
//                        paragraph = paragraph.replace(re3, "").replace(" ", "").replace("\u3000", "");

//                        // 本文が指定文字数以上の場合、指定文字数分のみを抽出
//                        paragraph = paragraph.size() > m_MaxParagraph ? paragraph.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : paragraph;
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "link") == 0) {
                        // ニュース記事のURLを取得
                        link = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // ニュース記事のURLからHTMLタグを解析した後、記事の概要を取得して指定文字数分のみ取得
                        QUrl url(link);
                        HtmlFetcher fetcher(m_MaxParagraph, this);

                        if (fetcher.fetch(url, true, QString(m_CNETParaXPath))) {
                            // ニュース記事の概要の取得に失敗した場合
                            bSkipNews = true;
                            break;
                        }

                        // ニュース記事の概要を取得
                        paragraph = fetcher.getParagraph();
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "date") == 0) {
                        date = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // 日付のフォーマットを"yyyy-MM-ddThh:mm+09:00"から"yyyy年M月d日 h時m分"へ変更
                        date = convertDate(date);

                        // ニュースの公開日を確認
                        auto isCheckDate = m_WithinHours == 0 ? isToday(date) : isHoursAgo(date);
                        if (!isCheckDate) {
                            bSkipNews = true;
                            break;
                        }
                    }
                }
                itemChild = itemChild->next;
            }

            // 今日のニュース記事ではない場合、または、指定時間以内のニュース記事ではない場合は無視
            if (bSkipNews) continue;

            // 既に書き込み済みの記事の場合は無視
            bool bWritten = false;
            for (auto &writtenArticle : m_WrittenArticles) {
                QString url = "";
                std::tie(std::ignore, std::ignore, url, std::ignore) = writtenArticle.getArticleData();

                if (url.compare(link, Qt::CaseSensitive) == 0) {
                    bWritten = true;
                    break;
                }
            }
            if (bWritten) continue;

            // 書き込む前の記事群
            Article article(title, paragraph, link, date);
            m_BeforeWritingArticles.append(article);

#ifdef _DEBUG
            qDebug() << "Title : " << title;
            qDebug() << "Paragraph : " << paragraph;
            qDebug() << "URL : " << link;
            qDebug() << "Date : " << date;
            qDebug() << "";
#endif
        }
        itemTagsforCNet(cur_node->children);
    }
}


void Runner::fetchHanJRSS()
{
    auto byteArray  = m_pReplyHanJ->readAll();
    auto xmlContent = byteArray.constData();

    // libxml2の初期化
    xmlInitParser();
    LIBXML_TEST_VERSION

    // メモリバッファからXMLをパース
    auto *doc = xmlReadMemory(xmlContent, byteArray.size(), "noname.xml", nullptr, 0);
    if (doc == nullptr) {
        std::cerr << "Failed to parse XML from memory" << std::endl;
        emit HanJfinished();

        return;
    }

    // ルート要素を取得
    auto *root_element = xmlDocGetRootElement(doc);

    // 各itemタグを処理
    itemTagsforHanJ(root_element);

    // ドキュメントを解放
    xmlFreeDoc(doc);

    // libxml2をクリーンアップ
    xmlCleanupParser();

    m_pReplyHanJ->deleteLater();

    emit HanJfinished();
}


void Runner::itemTagsforHanJ(xmlNode *a_node)
{
    for (auto cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST "item") == 0) {
            xmlNode *itemChild = cur_node->children;
            QString title       = "",
                    paragraph   = "",
                    link        = "",
                    date        = "";
            bool    bSkipNews   = false;

            while (itemChild) {
                if (itemChild->type == XML_ELEMENT_NODE) {
                    if (xmlStrcmp(itemChild->name, BAD_CAST "title") == 0) {
                        title = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "description") == 0) {
                        paragraph = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // 不要な文字を削除 (\n, \t) (ハンギョレジャパンのRSSの"description"には、不要な文字が含まれているため)
                        static QRegularExpression re1("[\t\n]", QRegularExpression::CaseInsensitiveOption);
                        paragraph = paragraph.replace(re1, "");

                        // tableタグを除去 (ハンギョレジャパンのRSSの"description"には、不要なHTMLタグが含まれているため)
                        static QRegularExpression re2("<table.*>.*</table>", QRegularExpression::DotMatchesEverythingOption);
                        paragraph.remove(re2);

                        // 不要な文字を削除 (半角 / 全角スペース等)
                        static QRegularExpression re3("[\\s]", QRegularExpression::CaseInsensitiveOption);
                        paragraph = paragraph.replace(re3, "").replace(" ", "").replace("\u3000", "");

                        // 本文が指定文字数以上の場合、指定文字数分のみを抽出
                        paragraph = paragraph.size() > m_MaxParagraph ? paragraph.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : paragraph;
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "link") == 0) {
                        link = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));
                        link = m_HanJTopURL + link;

                        // ニュース記事のURLからHTMLタグを解析した後、本文を取得して指定文字数分のみ取得 (現在は使用しない)
//                        QUrl url(link);
//                        HtmlFetcher fetcher(m_MaxParagraph, this);

//                        if (fetcher.fetch(url, true, QString("//head/meta[@property='og:description']/@content"))) {
//                            // 本文の取得に失敗した場合
//                            bSkipNews = true;
//                            break;
//                        }

//                        paragraph = fetcher.getParagraph();
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "pubDate") == 0) {
                        date = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // 日付のフォーマットをRFC 2822形式から"yyyy年M月d日 h時m分"へ変更
                        date = convertDateHanJ(date);

                        // ニュースの公開日を確認
                        auto isCheckDate = m_WithinHours == 0 ? isToday(date) : isHoursAgo(date);
                        if (!isCheckDate) {
                            bSkipNews = true;
                            break;
                        }
                    }
                }
                itemChild = itemChild->next;
            }

            // 今日のニュース記事ではない場合、または、指定時間以内のニュース記事ではない場合は無視
            if (bSkipNews) continue;

            // 既に書き込み済みの記事の場合は無視
            bool bWritten = false;
            for (auto &writtenArticle : m_WrittenArticles) {
                QString url = "";
                std::tie(std::ignore, std::ignore, url, std::ignore) = writtenArticle.getArticleData();

                if (url.compare(link, Qt::CaseSensitive) == 0) {
                    bWritten = true;
                    break;
                }
            }
            if (bWritten) continue;

            // 書き込む前の記事群
            Article article(title, paragraph, link, date);
            m_BeforeWritingArticles.append(article);

#ifdef _DEBUG
            qDebug() << "Title : " << title;
            qDebug() << "Paragraph : " << paragraph;
            qDebug() << "URL : " << link;
            qDebug() << "Date : " << date;
            qDebug() << "";
#endif
        }
        itemTagsforHanJ(cur_node->children);
    }
}


void Runner::fetchReutersRSS()
{
    auto byteArray  = m_pReplyReuters->readAll();
    auto xmlContent = byteArray.constData();

    // libxml2の初期化
    xmlInitParser();
    LIBXML_TEST_VERSION

    // メモリバッファからXMLをパース
    auto *doc = xmlReadMemory(xmlContent, byteArray.size(), "noname.xml", nullptr, 0);
    if (doc == nullptr) {
        std::cerr << "Failed to parse XML from memory" << std::endl;
        emit Reutersfinished();

        return;
    }

    // ルート要素を取得
    auto *root_element = xmlDocGetRootElement(doc);

    // 各itemタグを処理
    itemTagsforReuters(root_element);

    // ドキュメントを解放
    xmlFreeDoc(doc);

    // libxml2をクリーンアップ
    xmlCleanupParser();

    m_pReplyReuters->deleteLater();

    emit Reutersfinished();
}


void Runner::fetchTokyoNP()
{
    HtmlFetcher fetcher(m_MaxParagraph, this);

    // 東京新聞の総合ニュースからトップ記事を取得
    // 総合ニュースからニュース記事を取得しない場合は、設定ファイルの"topxpath"キーを空欄にすること
    if (!m_TokyoNPThumb.isEmpty()) {
        if (fetcher.fetchElement(QUrl(m_TokyoNPFetchURL), true, m_TokyoNPThumb, XML_TEXT_NODE)) {
            /// ヘッドラインニュースの記事の取得に失敗した場合
            std::cerr << QString("エラー : 東京新聞のヘッドラインニュース記事のURL取得に失敗").toStdString() << std::endl;
            return;
        }

        auto element = fetcher.GetElement();

        /// 末尾の半角スペースを削除
        if (element.endsWith(" ")) element.chop(1);

        auto link = m_TokyoNPTopURL + element;

        /// クエリパラメータを除去したURLを生成
        QUrl url(link);
        url.setQuery(QUrlQuery());
        link = url.toString();

        if (fetcher.fetchElement(QUrl(link), true, m_TokyoNPJSON, XML_CDATA_SECTION_NODE)) {
            /// ヘッドラインニュースの記事内容の取得に失敗した場合
            std::cerr << QString("エラー : 東京新聞のヘッドラインニュース記事内容の取得に失敗").toStdString() << std::endl;
            return;
        }

        element = fetcher.GetElement();

        /// 末尾の半角スペースを削除
        if (element.endsWith(" ")) element.chop(1);

        auto jsonData = element;
        QJsonDocument document = QJsonDocument::fromJson(jsonData.toUtf8());
        if(document.isNull()){
            std::cerr << QString("エラー : 東京新聞のヘッドラインニュース記事内容のJSONオブジェクト生成に失敗").toStdString() << std::endl;
            return;
        }

        if(!document.isObject()){
            std::cerr << QString("エラー : 東京新聞のヘッドラインニュース記事内容のJSONオブジェクトに異常があります").toStdString() << std::endl;
            return;
        }

        QJsonObject jsonObject = document.object();
        auto title      = jsonObject.value("headline").toString();

        auto paragraph  = jsonObject.value("description").toString();
        if (paragraph.endsWith("...")) {
            paragraph = (paragraph.size() - 3) > m_MaxParagraph ? paragraph.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : paragraph;
        }
        else {
            paragraph = paragraph.size() > m_MaxParagraph ? paragraph.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : paragraph;
        }

        auto date       = jsonObject.value("datePublished").toString();

        /// 日付のフォーマットをISO 8601形式から"yyyy年M月d日 h時m分"へ変更
        date            = convertDate(date);

        /// ニュースの公開日を確認
        auto isCheckDate = m_WithinHours == 0 ? isToday(date) : isHoursAgo(date);
        if (isCheckDate) {
            /// 既に書き込み済みの記事の場合は無視
            bool bWritten = false;
            for (auto &writtenArticle : m_WrittenArticles) {
                QString url = "";
                std::tie(std::ignore, std::ignore, url, std::ignore) = writtenArticle.getArticleData();

                if (url.compare(link, Qt::CaseSensitive) == 0) {
                    bWritten = true;
                    break;
                }
            }

            if (!bWritten) {
                /// 書き込む前の記事群
                Article article(title, paragraph, link, date);
                m_BeforeWritingArticles.append(article);

#ifdef _DEBUG
                qDebug() << "Title : " << title;
                qDebug() << "Paragraph : " << paragraph;
                qDebug() << "URL : " << link;
                qDebug() << "Date : " << date;
                qDebug() << "";
#endif
            }
        }
    }

    // 東京新聞のその他ニュース記事の取得
    if (fetcher.fetchElement(QUrl(m_TokyoNPFetchURL), true, m_TokyoNPNews, XML_TEXT_NODE)) {
        // その他ニュース記事のURL取得に失敗した場合
        std::cerr << QString("エラー : 東京新聞のニュース記事のURL取得に失敗").toStdString() << std::endl;
        return;
    }

    auto element = fetcher.GetElement();

    /// 末尾の半角スペースを削除
    if (element.endsWith(" ")) element.chop(1);

    /// リンク群からその他の各ニュース記事を取得
    auto elements = element.split(" ");
    foreach (const QString &item, elements) {
        auto link = m_TokyoNPTopURL + item;

        /// クエリパラメータを除去したURLを生成
        QUrl url(link);
        url.setQuery(QUrlQuery());
        link = url.toString();

        /// その他の各ニュース記事のURLにアクセスして、JSONオブジェクトの情報を取得
        if (fetcher.fetchElement(QUrl(link), true, m_TokyoNPJSON, XML_CDATA_SECTION_NODE)) {
            /// ヘッドラインニュースの記事の取得に失敗した場合
            std::cerr << QString("エラー : 東京新聞のニュース記事内容の取得に失敗 %1").arg(link).toStdString() << std::endl;
            return;
        }

        element = fetcher.GetElement();

        /// 末尾の半角スペースを削除
        if (element.endsWith(" ")) element.chop(1);

        /// JSONオブジェクトの情報からニュース記事のタイトル、本文の一部、公開日を取得
        auto jsonData = element;
        auto document = QJsonDocument::fromJson(jsonData.toUtf8());
        if(document.isNull()){
            std::cerr << QString("エラー : 東京新聞のニュース記事のJSONオブジェクト生成に失敗").toStdString() << std::endl;
            return;
        }

        if(!document.isObject()){
            std::cerr << QString("エラー : 東京新聞のニュース記事のJSONオブジェクトに異常があります").toStdString() << std::endl;
            return;
        }

        auto jsonObject = document.object();
        auto title      = jsonObject.value("headline").toString();

        auto paragraph  = jsonObject.value("description").toString();
        if (paragraph.endsWith("...")) {
            paragraph = (paragraph.size() - 3) >= m_MaxParagraph ? paragraph.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : paragraph;
        }
        else {
            paragraph = paragraph.size() > m_MaxParagraph ? paragraph.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : paragraph;
        }

        auto date       = jsonObject.value("datePublished").toString();

        /// 日付のフォーマットをISO 8601形式から"yyyy年M月d日 h時m分"へ変更
        date            = convertDate(date);

        /// ニュースの公開日を確認
        auto isCheckDate = m_WithinHours == 0 ? isToday(date) : isHoursAgo(date);
        if (isCheckDate) {
            /// 既に書き込み済みの記事の場合は無視
            bool bWritten = false;
            for (auto &writtenArticle : m_WrittenArticles) {
                QString url = "";
                std::tie(std::ignore, std::ignore, url, std::ignore) = writtenArticle.getArticleData();

                if (url.compare(link, Qt::CaseSensitive) == 0) {
                    bWritten = true;
                    break;
                }
            }

            if (!bWritten) {
                /// 書き込む前の記事群
                Article article(title, paragraph, link, date);
                m_BeforeWritingArticles.append(article);

#ifdef _DEBUG
                qDebug() << "Title : " << title;
                qDebug() << "Paragraph : " << paragraph;
                qDebug() << "URL : " << link;
                qDebug() << "Date : " << date;
                qDebug() << "";
#endif
            }
        }
    }

    return;
}


void Runner::JiJiFlashfetch()
{
    // 時事ドットコムの速報記事を取得するかどうかを確認
    if (!m_bJiJiFlash) {
        return;
    }

    // 現在の日時を取得して日付が変わっているかどうかを確認
    /// 日本のタイムゾーンを設定
    auto now = QDateTime::currentDateTimeUtc().toTimeZone(QTimeZone("Asia/Tokyo"));

    /// 現在の年月日のみを取得
    auto currentDate = now.date();
    auto date = QString("%1/%2/%3").arg(currentDate.year()).arg(currentDate.month()).arg(currentDate.day());

    /// 前回のニュース記事を取得した日付と比較
    if (m_LastUpdate.compare(date, Qt::CaseSensitive) != 0) {
        /// 日付が変わっている場合
        m_LastUpdate = date;

        /// メンバ変数m_WrittenArticlesから、書き込み済みの2日以上前の記事群を削除
        m_WrittenArticles.clear();

        /// ログファイルから、2日以上前の書き込み済みニュース記事を削除
        if (deleteLogNotToday()) {
            QCoreApplication::exit();
            return;
        }

        /// ログファイルから、今日と昨日の書き込み済みのニュース記事を取得
        /// また、取得した記事群のデータはメンバ変数m_WrittenArticlesに格納
        if (getDatafromWrittenLog()) {
            QCoreApplication::exit();
            return;
        }
    }

    // 設定ファイルの"update"キーを更新
    if (updateDateJson(m_LastUpdate)) {
        QCoreApplication::exit();
        return;
    }

    // 前回取得した書き込み前の記事群(選定前)を初期化
    m_BeforeWritingArticles.clear();

    if (m_stopRequested.load()) return;

    // 時事ドットコムから速報記事の取得
    JiJiFlash jijiFlash(m_MaxParagraph, m_JiJiFlashInfo, this);
    if (jijiFlash.FetchFlash()) {
        return;
    }

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;

    auto [title, paragraph, link, pubDate] = jijiFlash.getArticleData();

    // 速報記事の公開日を確認
    // 今日の速報記事ではない場合は無視
    auto isCheckDate = isToday(pubDate);
    if (!isCheckDate) {
        return;
    }

    // 既に書き込み済みの速報記事の場合は無視
    for (auto &writtenArticle : m_WrittenArticles) {
        QString url = "";
        std::tie(std::ignore, std::ignore, url, std::ignore) = writtenArticle.getArticleData();

        if (url.compare(link, Qt::CaseSensitive) == 0) {
            // 既に書き込み済みの記事の場合
            return;
        }
    }

#ifdef _DEBUG
    qDebug() << "書き込む速報記事";
    qDebug() << "Title : " << title;
    qDebug() << "URL : " << link;
    qDebug() << "Date : " << pubDate;
    qDebug() << "";
#endif

    // 速報記事は、タイトル --> 公開日 --> URL の順に並べて書き込む
    m_ThreadInfo.message = QString("%1%2%3").arg(QString(title + "\n"),
                                                 QString(pubDate + "\n\n"),
                                                 QString(link + "\n"));

    // 現在時刻をエポックタイムで取得
    auto epocTime = GetEpocTime();
    m_ThreadInfo.time = QString::number(epocTime);

    Poster poster(this);

    // 掲示板のクッキーを取得
    if (poster.fetchCookies(QUrl(m_RequestURL))) {
        // クッキーの取得に失敗した場合
        return;
    }

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR >= 1) && (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR <= 2)
    // qNewsFlash 0.1.0から0.2.xまでの機能

    // スレッドのタイトルを変更するかどうか (防弾嫌儲およびニュース速報(Libre)等の掲示板で使用可能)
    if (m_changeTitle) m_ThreadInfo.message.prepend("!chtt");

    // 既存のスレッドに書き込み
    if (poster.PostforWriteThread(QUrl(m_RequestURL), m_ThreadInfo)) {
        // 既存のスレッドへの書き込みに失敗した場合
        return;
    }
#elif QNEWSFLASH_VERSION_MAJOR > 0 || (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR >= 3)
    // qNewsFlash 0.3.0以降

    // 指定のスレッドがレス数の上限に達して書き込めない場合、スレッドを新規作成する
    HtmlFetcher fetcher(this);
    //auto iExpired = fetcher.checkUrlExistence(QUrl(m_ThreadURL), m_ExpiredElement, m_ExpiredXpath, m_ThreadInfo.shiftjis);
    auto iExpired = fetcher.checkUrlExistence(QUrl(m_ThreadURL), m_ThreadTitle, m_ExpiredXpath, m_ThreadInfo.shiftjis);
    if (iExpired == 0) {
        // 設定ファイルにあるスレッドのURLが生存している場合

        // ニュース記事を書き込むスレッドのレス数を取得
        auto ret = checkLastThreadNum();
        if (ret == -1) {
            // 最後尾のレス番号の取得に失敗した場合
            std::cerr << QString("エラー : レス数の取得に失敗").toStdString() << std::endl;
            return;
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

            if (poster.PostforCreateThread(QUrl(m_RequestURL), m_ThreadInfo)) {
                // スレッドの新規作成に失敗した場合
                return;
            }

            // 新規作成したスレッドのURLとスレッド番号を取得
            m_ThreadURL      = poster.GetNewThreadURL();
            m_ThreadInfo.key = poster.GetNewThreadNum();

            // スレッド情報 (スレッドのURLおよびスレッド番号) を設定ファイルに保存
            if (UpdateThreadJson(m_ThreadInfo.subject)) {
                // スレッド情報の保存に失敗
                return;
            }
        }
        else if (iExpired == 0) {
            // 既存のスレッドが存在する場合

            // スレッドのタイトルを変更するかどうか (防弾嫌儲およびニュース速報(Libre)等の掲示板で使用可能)
            if (m_changeTitle) m_ThreadInfo.message.prepend("!chtt");

            // 既存のスレッドに書き込み
            if (poster.PostforWriteThread(QUrl(m_RequestURL), m_ThreadInfo)) {
                // 既存のスレッドへの書き込みに失敗した場合
                return;
            }

            // スレッドのタイトルが正常に変更されているかどうかを確認
            if (m_changeTitle && !CompareThreadTitle(QUrl(m_ThreadURL), title)) {
                // スレッド情報 (スレッドのURLおよびスレッド番号) を設定ファイルに保存
                if (UpdateThreadJson(title)) {
                    // スレッド情報の保存に失敗
                    return;
                }
            }
            else {
                // スレッド情報 (スレッドのURLおよびスレッド番号) を設定ファイルに保存
                if (UpdateThreadJson(m_ThreadTitle)) {
                    // スレッド情報の保存に失敗
                    return;
                }
            }
        }
    }
    else if (iExpired == 1) {
        // 設定ファイルにあるスレッドのURLが存在しない、または、落ちている場合 (スレッドを新規作成する)

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
        if (poster.PostforCreateThread(QUrl(m_RequestURL), m_ThreadInfo)) {
            // スレッドの新規作成に失敗した場合
            return;
        }

        // 新規作成したスレッドのURLとスレッド番号を取得
        m_ThreadURL      = poster.GetNewThreadURL();
        m_ThreadInfo.key = poster.GetNewThreadNum();

        // スレッド情報 (スレッドのURLおよびスレッド番号) を設定ファイルに保存
        if (UpdateThreadJson(m_ThreadInfo.subject)) {
            // スレッド情報の保存に失敗
            return;
        }
    }
    else {
        return;
    }
#endif

    // 書き込み済みの記事を履歴として登録 (同じ記事を1日に2回以上書き込まないようにする)
    // ただし、2日前以上の書き込み済み記事の履歴は削除する
    Article article(title, paragraph, link, pubDate);
    m_WrittenArticles.append(article);

    // ログファイルに書き込む
    if (writeLog(article)) {
        QCoreApplication::exit();
        return;
    }

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;

    return;
}


void Runner::itemTagsforReuters(xmlNode *a_node)
{
    for (auto cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST "item") == 0) {
            xmlNode *itemChild = cur_node->children;
            QString title       = "",
                    paragraph   = "",
                    link        = "",
                    date        = "";
            bool    bSkipNews   = false;

            while (itemChild) {
                if (itemChild->type == XML_ELEMENT_NODE) {
                    if (xmlStrcmp(itemChild->name, BAD_CAST "title") == 0) {
                        title = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "link") == 0) {
                        // ニュース記事のURLを取得
                        link = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // ニュース記事のURLからHTMLタグを解析した後、記事の概要を取得して指定文字数分のみ取得
                        QUrl url(link);
                        HtmlFetcher fetcher(m_MaxParagraph, this);

                        if (fetcher.fetch(url, true, m_ReutersParaXPath)) {
                            // 本文の取得に失敗した場合
                            bSkipNews = true;
                            break;
                        }

                        // ニュース記事の概要を取得
                        paragraph = fetcher.getParagraph();
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "date") == 0) {
                        date = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // 日付のフォーマットをISO 8601形式から"yyyy年M月d日 h時m分"へ変更
                        date = convertDate(date);

                        // ニュースの公開日を確認
                        auto isCheckDate = m_WithinHours == 0 ? isToday(date) : isHoursAgo(date);
                        if (!isCheckDate) {
                            bSkipNews = true;
                            break;
                        }
                    }
                }
                itemChild = itemChild->next;
            }

            // 今日のニュース記事ではない場合、または、指定時間以内のニュース記事ではない場合は無視
            if (bSkipNews) continue;

            // 既に書き込み済みの記事の場合は無視
            bool bWritten = false;
            for (auto &writtenArticle : m_WrittenArticles) {
                QString url = "";
                std::tie(std::ignore, std::ignore, url, std::ignore) = writtenArticle.getArticleData();

                if (url.compare(link, Qt::CaseSensitive) == 0) {
                    bWritten = true;
                    break;
                }
            }
            if (bWritten) continue;

            // ロイター通信のRSSでは、1つのRSSに同じ記事が複数存在する場合がある
            // そのため、同じ記事が存在するかどうか確認して、存在する場合は無視する
            bool bIdenticalArticle = false;
            for (auto &beforeArticle : m_BeforeWritingArticles) {
                QString url = "";
                std::tie(std::ignore, std::ignore, url, std::ignore) = beforeArticle.getArticleData();

                if (url.compare(link, Qt::CaseSensitive) == 0) {
                    bIdenticalArticle = true;
                    break;
                }
            }
            if (bIdenticalArticle) continue;

            // 書き込む前の記事群
            Article article(title, paragraph, link, date);
            m_BeforeWritingArticles.append(article);

#ifdef _DEBUG
            qDebug() << "Title : " << title;
            qDebug() << "Paragraph : " << paragraph;
            qDebug() << "URL : " << link;
            qDebug() << "Date : " << date;
            qDebug() << "";
#endif
        }
        itemTagsforReuters(cur_node->children);
    }
}


QString Runner::convertJPDate(QString &strDate)
{
    QString formattedDateString = "";

    // UTC時刻をQDateTime型に変換
    QDateTime utcDateTime = QDateTime::fromString(strDate, Qt::ISODate);
    utcDateTime.setTimeZone(QTimeZone("Asia/Tokyo"));

    // UTC時間を明示的に設定
    utcDateTime.setTimeSpec(Qt::UTC);

    // 日本のタイムゾーンオブジェクト
    QTimeZone japanTimeZone("Asia/Tokyo");

    // UTCから日本時間へ変換
    QDateTime japanDateTime = utcDateTime.toTimeZone(japanTimeZone);

    if (!japanDateTime.isValid()) {
        std::cerr << QString("日付の変換に失敗 (News API) : %1").arg(strDate).toStdString();
    }
    else {
        formattedDateString = japanDateTime.toString("yyyy年M月d日 H時m分");
    }

    return formattedDateString;
}


QString Runner::convertJPDateforKyodo(QString &strDate)
{
    QString formattedDateString = "";

    // 英語ロケールを生成
    QLocale englishLocale(QLocale::English, QLocale::UnitedStates);

    // ISO形式へ変換
    QDateTime utcDateTime = englishLocale.toDateTime(strDate, QString("ddd, dd MMM yyyy HH:mm:ss +0000"));

    // タイムゾーンをUTCとして明示的に指定
    utcDateTime.setTimeSpec(Qt::UTC);

    // 正常に変換されているかどうかを確認
    if (utcDateTime.isValid()) {
        // 日本のタイムゾーンを指定
        QTimeZone japanTimeZone = QTimeZone("Asia/Tokyo");

        // UTC時間を日本時間に変換
        QDateTime japanDateTime = utcDateTime.toTimeZone(japanTimeZone);

        // "yyyy年M月d日 H時m分"形式へ変換
        formattedDateString = japanDateTime.toString("yyyy年M月d日 H時m分");
    }
    else {
        std::cerr << QString("日付の変換に失敗 (News API) : %1").arg(strDate).toStdString();
    }

    return formattedDateString;
}


QString Runner::convertDate(QString &strDate)
{
    // ISO 8601形式 (YYYY-MM-DDTHH:mm:SS+XX:XX) の日時列を解析
    auto dateTime = QDateTime::fromString(strDate, Qt::ISODate);
    QString convertDate = "";

    if (!dateTime.isValid()) {
        std::cerr << QString("日付の変換に失敗 (時事ドットコム) : %1").arg(strDate).toStdString();
    }
    else {
        convertDate = dateTime.toString("yyyy年M月d日 H時m分");
    }

    return convertDate;
}


QString Runner::convertDateHanJ(QString &strDate)
{
    // RFC 2822形式の日時列を解析
    QDateTime dateTime = QDateTime::fromString(strDate, Qt::RFC2822Date);
    QString convertDate = "";

    if (!dateTime.isValid()) {
        std::cerr << QString("日付の変換に失敗 (ハンギョレジャパン) : %1").arg(strDate).toStdString();
    }
    else {
        // "yyyy年M月d日 H時m分"へ変換
        convertDate = dateTime.toString("yyyy年M月d日 H時m分");
    }

    return convertDate;
}


bool Runner::isToday(const QString &dateString)
{
    // 日付フォーマットを指定
    QString format = "yyyy年M月d日 H時m分";

    // 文字列からQDateTimeオブジェクトを生成
    auto inputDate = QDateTime::fromString(dateString, format);

    // 入力日付の変換が成功したかどうかを確認
    if (!inputDate.isValid()) {
        std::cerr << "入力された日付が無効です" << std::endl;

        return false;
    }

    // 現在の日時を取得 -> タイムゾーンを日本時間に設定 -> 日付のみを抽出
    QDateTime todayTime = QDateTime::currentDateTime();
    todayTime.setTimeZone(QTimeZone("Asia/Tokyo"));
    auto today = todayTime.date();

    // ニュース記事の日付が今日の記事かどうかを判断
    return inputDate.date() == today;
}


bool Runner::isHoursAgo(const QString &pubDate) const
{
    // 日付フォーマットを指定
    QString format = "yyyy年M月d日 H時m分";

    // 文字列からQDateTimeオブジェクトを生成
    auto convertPubDate = QDateTime::fromString(pubDate, format);

    // 入力日付の変換が成功したかどうかを確認
    if (!convertPubDate.isValid()) {
        std::cerr << "入力された日付が無効です" << std::endl;

        return false;
    }

    // 現在の日時を取得 -> タイムゾーンを日本時間に設定 -> 日付のみを抽出
    QDateTime todayTime = QDateTime::currentDateTime();
    todayTime.setTimeZone(QTimeZone("Asia/Tokyo"));

    // 3時間前の日時を計算
    QDateTime hoursAgo = todayTime.addSecs(- m_WithinHours * 60 * 60);

    // ニュース記事の公開日時が現在日時の3時間前以内にあるかどうかを確認
    bool isInTargetDateTime = (convertPubDate >= hoursAgo && convertPubDate <= todayTime);

    // ニュース記事の公開日時が3時間前以内の記事かどうかを判断
    return isInTargetDateTime;
}


// 現在のエポックタイム (UNIX時刻) を秒単位で取得する
qint64 Runner::GetEpocTime()
{
    // 現在の日時を取得
    QDateTime now = QDateTime::currentDateTime().toTimeZone(QTimeZone("Asia/Tokyo"));;

    // エポックタイム (UNIX時刻) を秒単位で取得
    qint64 epochTime = now.toSecsSinceEpoch();

    return epochTime;
}


// 文字列 %t をスレッドのタイトルへ変更
QString Runner::replaceSubjectToken(QString subject, QString title)
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


int Runner::getConfiguration(QString &filepath)
{
    // 設定ファイルのパスが空の場合
    if(filepath.isEmpty()) {
        std::cerr << QString("エラー : オプションが不明です").toStdString() + QString("\n").toStdString() +
                     QString("使用可能なオプションは次の通りです : --sysconf=<qNewsFlash.jsonのパス>").toStdString() << std::endl;
        return -1;
    }

    // 指定された設定ファイルが存在しない場合
    if(!QFile::exists(filepath)) {
        std::cerr << QString("エラー : 設定ファイルが存在しません : %1").arg(filepath).toStdString() << std::endl;
        return -1;
    }

    // 設定ファイルを読み込む
    try {
        QFile File(filepath);
        if(!File.open(QIODevice::ReadOnly | QIODevice::Text)) {
            std::cerr << QString("エラー : 設定ファイルのオープンに失敗しました %1").arg(File.errorString()).toStdString() << std::endl;
            return -1;
        }

        auto byaryJson = File.readAll();

        // qNewsFlash.jsonファイルの設定を取得
        auto JsonDocument = QJsonDocument::fromJson(byaryJson);
        auto JsonObject   = JsonDocument.object();

        // News APIのニュース記事
        auto NewsAPIObject  = JsonObject["newsapi"].toObject();       /// News APIのニュースオブジェクトの取得
        m_bNewsAPI          = NewsAPIObject["enable"].toBool(false);  /// News APIの有効 / 無効
        m_API               = NewsAPIObject["api"].toString("");      /// News APIのAPIキー
        m_NewsAPIRSS        = NewsAPIObject["rss"].toString("");      /// News APIからニュース記事を取得するためのRSS (URL)
        auto excludes       = NewsAPIObject["exclude"].toArray();     /// 排除する特定メディア
        for (auto exclude : excludes) {
            m_ExcludeMedia.append(exclude.toString());
        }

        // 時事ドットコムのニュース記事
        auto JiJiObject     = JsonObject["jiji"].toObject();          /// 時事ドットコムのニュースオブジェクトの取得
        m_bJiJi             = JiJiObject["enable"].toBool(true);      /// 時事ドットコムの有効 / 無効
        m_JiJiRSS           = JiJiObject["rss"].toString("");         /// 時事ドットコムからニュース記事を取得するためのRSS (URL)

        // 時事ドットコムの速報ニュース記事
        auto jijiFlashObject  = JsonObject["jijiflash"].toObject();       /// 時事ドットコムの速報ニュースオブジェクトの取得
        m_bJiJiFlash          = jijiFlashObject["enable"].toBool(false);  /// 時事ドットコムの速報ニュースの有効 / 無効
        if (m_bJiJiFlash) {
            /// 時事ドットコムの速報ニュースを取得する時間間隔 (未指定の場合、インターバルは10[分])
            /// ただし、1分未満には設定できない (1分未満に設定した場合は、1分に設定する)
            auto jijiInterval            = jijiFlashObject["interval"].toString("600");
            bool ok;
            m_JiJiinterval = jijiInterval.toULongLong(&ok);
            if (!ok) {
                std::cerr << QString("警告 : 設定ファイルのjijiflash:intervalキーの値が不正です").toStdString() << std::endl;
                std::cerr << QString("更新時間の間隔は、自動的に10分に設定されます").toStdString() << std::endl;

                m_JiJiinterval = 10 * 60 * 1000;
            }
            else {
                if (m_JiJiinterval == 0) {
                    std::cerr << "インターバルの値が未指定もしくは0のため、10[分]に設定されます" << std::endl;
                    m_JiJiinterval = 10 * 60 * 1000;
                }
                else if (m_JiJiinterval < (1 * 60)) {
                    std::cerr << "インターバルの値が1[分]未満のため、1[分]に設定されます" << std::endl;
                    m_JiJiinterval = 1 * 60 * 1000;
                }
                else if (m_JiJiinterval < 0) {
                    std::cerr << "インターバルの値が不正です" << std::endl;
                    return -1;
                }
                else {
                    m_JiJiinterval *= 1000;
                }
            }

            /// 時事ドットコムの速報記事の基準となるURL
            m_JiJiFlashInfo.BasisURL     = jijiFlashObject["basisurl"].toString("");

            /// 時事ドットコムの速報記事の一覧が存在するURL
            m_JiJiFlashInfo.FlashUrl     = jijiFlashObject["flashurl"].toString("");

            /// 時事ドットコムの速報記事が記載されているURLから速報記事のリンクを取得するXPath
            m_JiJiFlashInfo.FlashXPath   = jijiFlashObject["flashxpath"].toString("");

            /// 時事ドットコムの各速報記事のURLから記事のタイトルを取得するXPath
            m_JiJiFlashInfo.TitleXPath   = jijiFlashObject["titlexpath"].toString("");

            /// 時事ドットコムの各速報記事のURLから記事の本文を取得するXPath
            m_JiJiFlashInfo.ParaXPath    = jijiFlashObject["paraxpath"].toString("");

            /// 時事ドットコムの各速報記事のURLから記事の公開日を取得するXPath
            m_JiJiFlashInfo.PubDateXPath = jijiFlashObject["pubdatexpath"].toString("");

            /// 時事ドットコムの各速報記事のURLから"<この速報の記事を読む>"の部分のリンクを取得するXPath
            m_JiJiFlashInfo.UrlXPath     = jijiFlashObject["urlxpath"].toString("");
        }

        // 共同通信のニュース記事
        auto KyodoObject    = JsonObject["kyodo"].toObject();         /// 共同通信のニュースオブジェクトの取得
        m_bKyodo            = KyodoObject["enable"].toBool(true);     /// 共同通信の有効 / 無効
        m_KyodoRSS          = KyodoObject["rss"].toString("");        /// 共同通信からニュース記事を取得するためのRSS (URL)
        m_NewsOnly          = KyodoObject["newsonly"].toBool(true);   /// 共同通信から取得するニュース記事の種類をニュースのみに絞るかどうか
                                                                      /// ニュース以外の記事では、ビジネス関連やライフスタイル等の記事がある

        // 朝日新聞デジタルのニュース記事
        auto AsahiObject    = JsonObject["asahi"].toObject();         /// 朝日新聞デジタルのニュースオブジェクトの取得
        m_bAsahi            = AsahiObject["enable"].toBool(false);    /// 朝日新聞デジタルの有効 / 無効
        m_AsahiRSS          = AsahiObject["rss"].toString("");        /// 朝日新聞デジタルからニュース記事を取得するためのRSS (URL)

        // ハンギョレジャパンのニュース記事
        auto HanJObject     = JsonObject["hanj"].toObject();          /// ハンギョレジャパンのニュースオブジェクトの取得
        m_bHanJ             = HanJObject["enable"].toBool(false);     /// ハンギョレジャパンの有効 / 無効
        m_HanJRSS           = HanJObject["rss"].toString("");         /// ハンギョレジャパンからニュース記事を取得するためのRSS (URL)
        m_HanJTopURL        = HanJObject["toppage"].toString("");     /// ハンギョレジャパンのトップページのURL
                                                                      /// ハンギョレジャパンでは、現在、トップページを基準にニュース記事のURLが存在する
        if (m_bHanJ && (m_HanJRSS.isEmpty() || m_HanJTopURL.isEmpty())) {
            std::cerr << QString("エラー : ハンギョレジャパンが有効になっていますが、\"rss\"もしくは\"toppage\"の値が設定されていません").toStdString() << std::endl;
        }

        // ロイター通信のニュース記事
        auto ReutersObject  = JsonObject["reuters"].toObject();         /// ロイター通信のニュースオブジェクトの取得
        m_bReuters          = ReutersObject["enable"].toBool(false);    /// ロイター通信の有効 / 無効
        m_ReutersRSS        = ReutersObject["rss"].toString("");        /// ロイター通信からニュース記事を取得するためのRSS (URL)
        m_ReutersParaXPath  = ReutersObject["paraxpath"].toString("");  /// ロイター通信からニュース記事の概要を取得するためのXPath式
        if (m_bReuters && (m_ReutersRSS.isEmpty() || m_ReutersParaXPath.isEmpty())) {
            std::cerr << QString("エラー : ロイター通信が有効になっていますが、\"rss\"もしくは\"paraxpath\"の値が設定されていません").toStdString() << std::endl;
        }

        // CNET Japanのニュース記事
        auto CNETObject     = JsonObject["cnet"].toObject();           /// CNET Japanのニュースオブジェクトの取得
        m_bCNet             = CNETObject["enable"].toBool(false);      /// CNET Japanの有効 / 無効
        m_CNETRSS           = CNETObject["rss"].toString("");          /// CNET Japanからニュース記事を取得するためのRSS (URL)
        m_CNETParaXPath     = CNETObject["paraxpath"].toString("");    /// CNET Japanからニュース記事の概要を取得するためのXPath式
        if (m_bCNet && (m_CNETRSS.isEmpty() || m_CNETParaXPath.isEmpty())) {
            std::cerr << QString("エラー : CNET Japanが有効になっていますが、\"rss\"もしくは\"paraxpath\"の値が設定されていません").toStdString() << std::endl;
        }

        // 東京新聞のニュースオブジェクトの取得
        auto tokyoNPObject  = JsonObject["tokyonp"].toObject();
        m_bTokyoNP          = tokyoNPObject["enable"].toBool(false);  /// 東京新聞の有効 / 無効
        m_TokyoNPTopURL     = tokyoNPObject["toppage"].toString("");  /// 東京新聞のトップページのURL
        if (m_TokyoNPTopURL.isEmpty()) m_TokyoNPTopURL = QString("https://www.tokyo-np.co.jp");

        /// 東京新聞からニュース記事を取得するページのURL
        /// 例1. 総合ニュースを取得する場合 : https://www.tokyo-np.co.jp
        /// 例2. 政治ニュースのみを取得する場合 : https://www.tokyo-np.co.jp/n/politics
        /// 例3. 国際ニュースのみを取得する場合 : https://www.tokyo-np.co.jp/n/world
        m_TokyoNPFetchURL   = tokyoNPObject["fetchurl"].toString("");
        if (m_TokyoNPFetchURL.isEmpty()) m_TokyoNPFetchURL = QString("https://www.tokyo-np.co.jp");

        /// 東京新聞のヘッドライン取得用XPath
        m_TokyoNPThumb      = tokyoNPObject["topxpath"].toString("");

        /// 東京新聞のその他ニュース記事の取得用XPath
        m_TokyoNPNews       = tokyoNPObject["newsxpath"].toString("");

        /// 東京新聞のその他ニュース記事の情報を取得するためのJSONオブジェクト用XPath
        m_TokyoNPJSON       = tokyoNPObject["jsonpath"].toString("");

        // メンバ変数m_intervalの値を使用して自動的にニュース記事を取得するかどうか
        // ワンショット機能の有効 / 無効
        m_AutoFetch         = JsonObject["autofetch"].toBool(true);

        // ニュース記事を取得する間隔
        auto interval    = JsonObject["interval"].toString("60 * 1000 * 30");
        bool ok;
        m_interval = interval.toULongLong(&ok);
        if (!ok) {
            std::cerr << QString("警告 : 設定ファイルのintervalキーの値が不正です").toStdString() << std::endl;
            std::cerr << QString("更新時間の間隔は、自動的に30分に設定されます").toStdString() << std::endl;

            m_interval = 30 * 60 * 1000;
        }
        else {
            if (m_interval == 0) {
                std::cerr << "インターバルの値が未指定もしくは0のため、30[分]に設定されます" << std::endl;
                m_interval = 60 * 1000 * 30;
            }
            else if (m_interval < (3 * 60)) {
                std::cerr << "インターバルの値が3[分]未満のため、3[分]に設定されます" << std::endl;
                m_interval = 3 * 60 * 1000;
            }
            else if (m_interval < 0) {
                std::cerr << "インターバルの値が不正です" << std::endl;
                return -1;
            }
            else {
                m_interval *= 1000;
            }
        }

        // 本文の一部を抜粋する場合の最大文字数
        auto maxParagraph  = JsonObject["maxpara"].toString("100");
        m_MaxParagraph = maxParagraph.toLongLong(&ok);
        if (!ok) {
            std::cerr << QString("警告 : 設定ファイルのmaxparaキーの値が不正です").toStdString() << std::endl;
            std::cerr << QString("本文の一部を抜粋する場合の最大文字数は、自動的に100文字に設定されます").toStdString() << std::endl;

            m_MaxParagraph = 100;
        }

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR >= 1) && (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR <= 2)
        // qNewsFlash 0.1.0から0.2.xまでの機能

        // 掲示板へPOSTデータを送信するURL
        m_RequestURL            = JsonObject["requesturl"].toString("");

        // スレッド情報
        m_ThreadInfo.subject    = JsonObject["subject"].toString("");
        m_ThreadInfo.from       = JsonObject["from"].toString("");
        m_ThreadInfo.mail       = JsonObject["mail"].toString("");      // メール欄
        m_ThreadInfo.bbs        = JsonObject["bbs"].toString("");       // BBS名
        m_ThreadInfo.key        = JsonObject["key"].toString("");       // スレッド番号
        m_ThreadInfo.shiftjis   = JsonObject["shiftjis"].toBool(true);  // Shift-JISの有効 / 無効

        // スレッドのタイトルを変更するかどうか
        // この機能は、防弾嫌儲およびニュース速報(Libre)等のスレッドのタイトルが変更できる掲示板で使用可能
        m_changeTitle    = JsonObject["chtt"].toBool(false);
#elif QNEWSFLASH_VERSION_MAJOR > 0 || (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR >= 3)
        // qNewsFlash 0.3.0以降

        // スレッド情報
        auto threadObject  = JsonObject["thread"].toObject();

        /// 掲示板へPOSTデータを送信するURL
        m_RequestURL       = threadObject["requesturl"].toString("");
        if (m_RequestURL.isEmpty()) {
            std::cerr << QString("エラー : 掲示板へPOSTデータを送信するURLが空欄です").toStdString() << std::endl;
            return -1;
        }

        /// 書き込むスレッドのURL
        m_ThreadURL        = threadObject["threadurl"].toString("");
        if (m_ThreadURL.isEmpty()) {
            std::cout << QString("スレッドのURLが空欄のため、スレッドを新規作成します").toStdString() << std::endl;
        }

        /// 書き込み済みのスレッドのタイトル
        m_ThreadTitle      = threadObject["threadtitle"].toString("");

        /// スレッドのレス数を取得するためのXPath
        m_ThreadXPath      = threadObject["threadxpath"].toString("/html/body//div/dl[@class='thread']//div/@id");
        if (m_ThreadXPath.isEmpty()) {
            std::cerr << QString("エラー : スレッドのレス数を取得するためのXPathが空欄です").toStdString() << std::endl;
            return -1;
        }

        /// スレッドの最大レス数
        m_maxThreadNum          = threadObject["max"].toInt(1000);

        /// POSTデータに関する情報
        auto subject            = threadObject["subject"].toString("");   // スレッドのタイトル (スレッドを新規作成する場合)
                                                                          // 空欄の場合、スレッドのタイトルは取得したニュース記事のタイトルとなる
        /// エスケープされた%tトークンを一時的に保護
        QString tempToken = "__TEMP__";
        subject.replace("%%t", tempToken);

        if (subject.count("%t") > 1) {
            /// %tトークンが複数存在する場合はエラー
            std::cerr << QString("エラー : \"%t\"トークンが複数存在します").toStdString() << std::endl;
            return -1;
        }
        else {
            /// 保護されたトークンを元に戻す
            subject.replace(tempToken, "%%t");
            m_ThreadInfo.subject = subject;
        }

        m_ThreadInfo.from       = threadObject["from"].toString("");      // 名前欄
        m_ThreadInfo.mail       = threadObject["mail"].toString("");      // メール欄
        m_ThreadInfo.bbs        = threadObject["bbs"].toString("");       // BBS名
        m_ThreadInfo.key        = threadObject["key"].toString("");       // スレッド番号
        m_ThreadInfo.shiftjis   = threadObject["shiftjis"].toBool(true);  // Shift-JISの有効 / 無効

        // Webページから一意のタグの値を取得
        // この値を確認して、スレッドが落ちているかどうかを判断する
        // デフォルトの設定では、スレッドが落ちている状態のスレッドタイトル名
        m_ExpiredElement     = threadObject["expiredelement"].toString("");

        // スレッドが落ちた時のスレッドタイトル名を取得するXPath
        m_ExpiredXpath       = threadObject["expiredxpath"].toString("");

        // 新規スレッドのタイトル名とスレッドが落ちた時のスレッドタイトル名が同じ場合はエラー
//        if (m_ThreadInfo.subject.compare(m_ExpiredElement, Qt::CaseSensitive) == 0) {
//            std::cerr << QString("エラー : 新規スレッドのタイトル名とスレッドが落ちている状態のエレメント名には、別の文言を指定してください").toStdString() << std::endl;
//            return -1;
//        }

        // スレッドのタイトルを変更するかどうか
        // この機能は、防弾嫌儲およびニュース速報(Libre)等のスレッドのタイトルが変更できる掲示板で使用可能
        m_changeTitle    = threadObject["chtt"].toBool(false);
#endif

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
        // qNewsFlash 0.1.0未満の機能
        // スレッド書き込み用のJSONファイルのパス
        auto writeFile = JsonObject["writefile"].toString("/tmp/qNewsFlashWrite.json");

        /// ファイルの情報を取得
        QFileInfo fileInfo(writeFile);

        /// ディレクトリのパスを取得
        QString dirPath = fileInfo.absolutePath();

        /// ディレクトリの存在を確認
        QDir dir(dirPath);
        if (!dir.exists()) {
            std::cerr << QString("エラー : 書き込み用JSONファイルを保存するディレクトリが存在しません %1").arg(dirPath).toStdString() << std::endl;
            return -1;
        }

        m_WriteFile = writeFile;
#endif

        // 公開日がn時間前以内のニュース記事を取得する設定
        // 0 : 無効
        // 1 - 24 : 公開日がn時間以内のニュース記事を取得
        // それ以外 : 無効
        auto withinhours = JsonObject["withinhours"].toString("0");
        m_WithinHours = withinhours.toInt(&ok);
        if (!ok || m_WithinHours < 0 || m_WithinHours > 24) {
            std::cerr << QString("警告 : 設定ファイルのwithinhoursキーの値が不正です").toStdString() << std::endl;
            std::cerr << QString("公開日が本日付けのみのニュース記事を取得します").toStdString() << std::endl;

            m_WithinHours = 0;
        }

        // スレッド書き込み済み(ログ用)のJSONファイルのパス
        auto logFile = JsonObject["logfile"].toString("/var/log/qNewsFlash_log.json");
        m_LogFile    = logFile;

        // 最後にニュース記事群を取得した日付
        auto update     = JsonObject["update"].toString("");

        /// 日付の文字列を解析
        /// 第2引数には、解析したい日付のフォーマットを指定
        /// 生成したQDateオブジェクトが正しい日付を表しているかどうかを確認
        QDate date = QDate::fromString(update, "yyyy/M/d");
        if (!date.isValid()) {
            /// 日付のフォーマットが不正の場合は、今日の年月日を使用
            auto now = QDateTime::currentDateTimeUtc().toTimeZone(QTimeZone("Asia/Tokyo"));
            auto currentDate = now.date();
            m_LastUpdate = QString("%1/%2/%3").arg(currentDate.year()).arg(currentDate.month()).arg(currentDate.day());
        }
        else {
            m_LastUpdate = update;
        }

        File.close();
    }
    catch(QException &ex) {
        std::cerr << QString("エラー : %1").arg(ex.what()).toStdString() << std::endl;
        return -1;
    }

    return 0;
}


int Runner::updateDateJson(const QString &currentDate)
{
    // 設定ファイルの読み込み
    QFile File(m_SysConfFile);
    if (!File.open(QIODevice::ReadOnly)) {
        std::cerr << QString("エラー : 設定ファイルのオープンに失敗 %1").arg(m_SysConfFile).toStdString() << std::endl;
        return -1;
    }

    QJsonDocument doc;
    try {
        // 設定ファイルの内容を読み込む
        doc = QJsonDocument::fromJson(File.readAll());
    }
    catch(QException &ex) {
        File.close();

        std::cerr << QString("エラー : %1").arg(ex.what()).toStdString() << std::endl;
        return -1;
    }

    // 設定ファイルを閉じる
    File.close();

    if (!doc.isObject()) {
        std::cerr << QString("エラー : 設定ファイルがオブジェクトではありません").toStdString() << std::endl;
        return -1;
    }

    QJsonObject obj = doc.object();

    // 設定ファイルの"update"キーを更新
    obj["update"] = currentDate;
    doc.setObject(obj);

    // 更新されたJSONオブジェクトをファイルに書き戻す
    if (!File.open(QIODevice::WriteOnly)) {
        std::cerr << QString("エラー : 設定ファイルのオープンに失敗 %1").arg(m_SysConfFile).toStdString() << std::endl;
        return -1;
    }

    // 設定ファイルを更新
    try {
        File.write(doc.toJson());
    }
    catch(QException &ex) {
        File.close();

        std::cerr << QString("エラー : %1").arg(ex.what()).toStdString() << std::endl;
        return -1;
    }

    // 設定ファイルを閉じる
    File.close();

    return 0;
}


Article Runner::selectArticle()
{
    // CPUのタイムスタンプカウンタ(TSC)をハッシュ化した数値をXorshiftしてシード値を生成
    // 生成したシード値を使用して乱数を生成 (一様分布)
    // 乱数は、0〜(取得した記事の数 -1)までの値をとる
    RandomGenerator randomObj;
    int randomValue = randomObj.Generate(m_BeforeWritingArticles.size());

#ifdef _DEBUG
    // 生成された乱数を出力
    std::cout << QString("生成された乱数 : この値を取得したニュース記事群の配列のインデックス値とする : %1").arg(randomValue).toStdString() << std::endl << std::endl;
#endif

    return m_BeforeWritingArticles.at(randomValue);
}


#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
// qNewsFlash 0.1.0未満の機能
int Runner::writeJSON(Article &article)
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


int Runner::truncateJSON()
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


int Runner::writeLog(Article &article)
{
    // ログファイルを開く (読み込み用)
    QFile File(m_LogFile);
    QByteArray jsonData = {};
    if (!File.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << QString("エラー : ログファイルのオープンに失敗 %1").arg(File.errorString()).toStdString() << std::endl;
        return -1;
    }
    else {
        // ログファイルからJSONデータを読み込む
        try {
            jsonData = File.readAll();
        }
        catch (QException &ex) {
            std::cerr << QString("エラー : ファイルの読み込みに失敗 %1").arg(File.errorString()).toStdString() << std::endl;
            return -1;
        }

        // ファイルを閉じる
        File.close();
    }

    // 追記する記事
    auto [title, paragraph, url, date] = article.getArticleData();

    QJsonDocument jsonDoc(QJsonDocument::fromJson(jsonData));
    if (!jsonDoc.isArray()) {
        jsonDoc.array() = QJsonArray();
    }
    auto jsonArray = jsonDoc.array();

    QJsonObject newObject;
    newObject["title"]      = title;
    newObject["paragraph"]  = paragraph;
    newObject["url"]        = url;
    newObject["date"]       = date;

    // 書き込み済みの記事を追記
    jsonArray.append(newObject);

    // ログファイルを再度開く (書き込み用)
    if (!File.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << QString("エラー : ファイルのオープンに失敗").toStdString() << std::endl;
        return -1;
    }
    else {
        // ログファイルへ書き込み
        try {
            QJsonDocument appendDoc(jsonArray);
            File.write(appendDoc.toJson());
        }
        catch (QException &ex) {
            std::cerr << QString("エラー : ファイルの書き込みに失敗").toStdString() << std::endl;
            return -1;
        }

        // ログファイルを閉じる
        File.close();
    }

    return 0;
}


int Runner::setLogFile(QString &filepath)
{
    // 書き込み済み(ログ用)のJSONファイルの情報を取得
    QFileInfo fileInfo(filepath);

    // 書き込み済み(ログ用)のJSONファイルを保存するディレクトリのパスを取得
    auto dirPath = fileInfo.absolutePath();

    // 書き込み済み(ログ用)のJSONファイルを保存するディレクトリを確認
    QDir logDir(dirPath);
    if (!logDir.exists()) {
        // ディレクトリが存在しない場合は作成
        std::cout << QString("ログディレクトリが存在しないため作成します %1").arg(logDir.absolutePath()).toStdString() << std::endl;

        if (!logDir.mkpath(".")) {
            std::cerr << QString("エラー : ログディレクトリの作成に失敗 %1").arg(logDir.absolutePath()).toStdString() << std::endl;
            return -1;
        }
    }

    // 書き込み済み(ログ用)のJSONファイルが存在しない場合は空のログファイルを作成
    QFile File(filepath);
    if (!File.exists()) {
        std::cout << QString("ログファイルが存在しないため作成します %1").arg(filepath).toStdString() << std::endl;

        if (File.open(QIODevice::WriteOnly)) {
            File.close();
        }
        else {
            std::cerr << QString("エラー : ログファイルを作成に失敗 %1").arg(File.errorString()).toStdString() << std::endl;
            return -1;
        }
    }

    return 0;
}


#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
int Runner::setLogFile()
{
    QString logDir  = "";
    QString logFile = "";

    if (m_User.compare("root", Qt::CaseSensitive) == 0) {
        // スーパーユーザの場合は、/var/log/qNewsFlash_log.jsonファイル
        logDir  = QString("/var/log");
        logFile = QString("/var/log/qNewsFlash_log.json");
    }
    else {
        // 一般ユーザの場合は、~/.config/qNewsFlash/qNewsFlash_log.jsonファイル
        auto homePath   = QDir::homePath();
        logDir  = homePath + QDir::separator() + QString(".config") + QDir::separator() + QString("qNewsFlash");
        logFile = homePath + QDir::separator() + QString(".config") + QDir::separator() +
                  QString("qNewsFlash") + QDir::separator() + QString("qNewsFlash_log.json");
    }

    // ログファイルを保存するディレクトリを確認
    QDir dir(logDir);
    if (!dir.exists()) {
        // ディレクトリが存在しない場合は作成
        if (!dir.mkpath(".")) {
            std::cerr << QString("エラー : ログディレクトリの作成に失敗 %1").arg(logDir).toStdString() << std::endl;
            return -1;
        }
    }

    // ログファイルが存在しない場合は空のログファイルを作成
    QFile File(logFile);
    if (!File.exists()) {
        if (File.open(QIODevice::WriteOnly)) {
            File.close();
        }
        else {
            std::cerr << QString("エラー : ログファイルを作成に失敗 %1").arg(File.errorString()).toStdString() << std::endl;
            return -1;
        }
    }

    m_LogFile = logFile;

    return 0;
}
#endif


int Runner::deleteLogNotToday()
{
    // ログファイルを開く
    QFile File(m_LogFile);
    if (!File.open(QIODevice::ReadOnly)) {
        std::cerr << QString("エラー : ログファイルのオープンに失敗").toStdString() << std::endl;
        return -1;
    }

    // ログファイルの読み込み
    QByteArray data = {};
    try {
        data = File.readAll();
    }
    catch (QException &ex) {
        File.close();

        std::cerr << QString("エラー : ファイルの読み込みに失敗 %1").arg(File.errorString()).toStdString() << std::endl;
        return -1;
    }

    // ログファイルをクローズ
    File.close();

    // 公開日が2日以上前の書き込み済みニュース記事をログファイルから削除
    QJsonDocument doc(QJsonDocument::fromJson(data));
    QJsonArray array = doc.array();

    /// 保存する書き込み済み記事
    QJsonArray newLogArray = {};

    /// 日本のタイムゾーンを設定して、今日の日付を取得
    QTimeZone timeZone("Asia/Tokyo");
    QDate currentDate = QDate::currentDate();

    for (auto && i : array) {
        auto obj = i.toObject();

        /// " h時m分"の部分を除去
        auto pubDateString = obj["date"].toString().split(" ")[0];
        auto pubDate = QDate::fromString(pubDateString, "yyyy年M月d日");

        // 現在の日付と公開日の日付の差
        auto daysDiff = pubDate.daysTo(currentDate);

        // ニュース記事の公開日が前日までの場合は残す
        if (daysDiff >= 0 && daysDiff <= 1) {
            newLogArray.append(obj);
        }
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

    // ログファイルを再度開いて内容を変更
    QJsonDocument saveDoc(newLogArray);
    if (!File.open(QIODevice::WriteOnly)) {
        std::cerr << QString("エラー : ログファイルのオープンに失敗").toStdString() << std::endl;
        return -1;
    }

    // ログファイルへ書き込み
    try {
        File.write(saveDoc.toJson());
    }
    catch (QException &ex) {
        File.close();

        std::cerr << QString("エラー : ログファイルの書き込みに失敗").toStdString() << std::endl;
        return -1;
    }

    // ログファイルを閉じる
    File.close();

    return 0;
}


int Runner::getDatafromWrittenLog()
{
    // ログファイルを開く
    QFile File(m_LogFile);
    if (!File.open(QIODevice::ReadOnly)) {
        std::cerr << QString("エラー : ログファイルのオープンに失敗").toStdString() << std::endl;
        return -1;
    }

    // ログファイル内の書き込み済みの記事群を読み込み
    QByteArray data = {};
    try {
        data = File.readAll();
    }
    catch (QException &ex) {
        File.close();

        std::cerr << QString("エラー : ログファイルの読み込みに失敗 %1").arg(File.errorString()).toStdString() << std::endl;
        return -1;
    }

    // ログファイルをクローズ
    File.close();

    // ログファイル内にある書き込み済みの記事群を取得 (本日分のみ)
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (!jsonDoc.isArray()) {
        std::cerr << QString("エラー : ログファイルのフォーマットが不正です").toStdString() << std::endl;
        return -1;
    }

    QJsonArray jsonArray = jsonDoc.array();
    for (const auto &value : jsonArray) {
        QJsonObject obj = value.toObject();

        Article article(obj["title"].toString(), obj["paragraph"].toString(), obj["url"].toString(), obj["date"].toString());
        m_WrittenArticles.append(article);
    }

    return 0;
}


// 書き込むスレッドのレス数が上限に達しているかどうかを確認
int Runner::checkLastThreadNum()
{
    HtmlFetcher fetcher(this);
    if (fetcher.fetchLastThreadNum(QUrl(m_ThreadURL), false, m_ThreadXPath, XML_TEXT_NODE)) {
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
    if (max >= m_maxThreadNum) {
        // 上限に達している場合
        return 1;
    }

    return 0;
}


// !chttコマンドでスレッドのタイトルが正常に変更されているかどうかを判断する
// !chttコマンドは、防弾嫌儲系のみ使用可能
// 0  : スレッドのタイトルが正常に変更された場合
// 1  : !chttコマンドが失敗している場合
// -1 : スレッドのタイトルの取得に失敗した場合
int Runner::CompareThreadTitle(const QUrl &url, const QString &title)
{
    // 過去のスレッドタイトルと変更後のスレッドタイトルが同一の場合
    if (title.compare(m_ThreadTitle, Qt::CaseSensitive) == 0) return 0;

    // スレッドから<title>タグをXPathを使用して抽出する
    HtmlFetcher fetcher(this);
    if (fetcher.extractThreadTitle(url, true, m_ExpiredXpath, m_ThreadInfo.shiftjis)) {
        // <title>タグの取得に失敗した場合
        std::cerr << QString("エラー : <title>タグの取得に失敗 - CompareThreadTitle()").toStdString() << std::endl;
        return -1;
    }

    auto ThreadTitle = fetcher.GetElement();
    if (ThreadTitle.compare(title, Qt::CaseSensitive) != 0) {
        // スレッドのタイトルが変更されていない場合
        // !chttコマンドが失敗している場合
        return 1;
    }

    return 0;
}


// スレッド情報 (スレッドのタイトル、スレッドのURL、スレッド番号) を設定ファイルに保存
int Runner::UpdateThreadJson(const QString &title)
{
    // 設定ファイルの読み込み
    QFile File(m_SysConfFile);
    if (!File.open(QIODevice::ReadOnly)) {
        std::cerr << QString("エラー : 設定ファイルのオープンに失敗 %1").arg(m_SysConfFile).toStdString() << std::endl;
        return -1;
    }

    QJsonDocument doc;
    try {
        // 設定ファイルの内容を読み込む
        doc = QJsonDocument::fromJson(File.readAll());
    }
    catch(QException &ex) {
        File.close();

        std::cerr << QString("エラー : %1").arg(ex.what()).toStdString() << std::endl;
        return -1;
    }

    // 設定ファイルを閉じる
    File.close();

    if (!doc.isObject()) {
        std::cerr << QString("エラー : 設定ファイルがオブジェクトではありません").toStdString() << std::endl;
        return -1;
    }

    QJsonObject obj = doc.object();

    // 設定ファイルの"thread"オブジェクトにある"key"および"threadurl"を更新
    QJsonObject threadObj       = obj.value("thread").toObject();
    threadObj["key"]            = m_ThreadInfo.key;
    threadObj["threadurl"]      = m_ThreadURL;
    threadObj["threadtitle"]    = title;
    obj["thread"]               = threadObj;

    doc.setObject(obj);

    // 更新されたJSONオブジェクトをファイルに書き戻す
    if (!File.open(QIODevice::WriteOnly)) {
        std::cerr << QString("エラー : 設定ファイルのオープンに失敗 %1").arg(m_SysConfFile).toStdString() << std::endl;
        return -1;
    }

    // 設定ファイルを更新
    try {
        File.write(doc.toJson());
    }
    catch(QException &ex) {
        File.close();

        std::cerr << QString("エラー : %1").arg(ex.what()).toStdString() << std::endl;
        return -1;
    }

    // 設定ファイルを閉じる
    File.close();

    return 0;
}


// [q]キーまたは[Q]キー ==> [Enter]キーを押下した場合、メインループを抜けて本ソフトウェアを終了する
void Runner::onReadyRead()
{
    // 標準入力から1行のみ読み込む
    QTextStream tstream(stdin);
    QString line = tstream.readLine();

    if (line.compare("q", Qt::CaseInsensitive) == 0) {
        m_stopRequested.store(true);

        QCoreApplication::exit();
        return;
    }
}
