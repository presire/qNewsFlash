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
#include <QLockFile>
#include <QException>
#include <iostream>
#include <utility>
#include "Runner.h"
#include "HtmlFetcher.h"
#include "RandomGenerator.h"
#include "CommandLineParser.h"


// メイン処理のコンストラクタ
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


// このソフトウェアを最初に実行する時にのみ実行するメイン処理
void Runner::run()
{
    // メイン処理

    // コマンドラインオプションの確認
    // 設定ファイルおよびバージョン情報
    m_args.removeFirst();   // プログラムのパスを削除

    CommandLineParser parser;
    parser.setApplicationDescription("qNewsFlashは、時事ドットコムや共同通信等のニュース記事を取得して、0ch系の掲示板に書き込むソフトウェアです");

    // --sysconf オプションを追加
    QCommandLineOption sysconfOption(QStringList() << "sysconf",
                                     "設定ファイル(.json)のパスを指定します",
                                     "confFilePath");
    parser.addOption(sysconfOption);

    // --version / -v オプションを追加
    QCommandLineOption versionOption(QStringList() << "version" << "v", "バージョン情報を表示します");
    parser.addOption(versionOption);

    // --help / -h オプションを追加
    QCommandLineOption helpOption(QStringList() << "help" << "h", "ヘルプ情報を表示します");
    parser.addOption(helpOption);

    // 未知のオプションを許可
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);

    // コマンドライン引数を解析
    parser.process(QCoreApplication::arguments());

    // 未知のオプションをチェック
    if (!parser.unknownOptionNames().isEmpty()) {
        std::cerr << QString("エラー : 不明なオプション %1").arg(parser.unknownOptionNames().join(", ")).toStdString() << std::endl;
        QCoreApplication::exit();
        return;
    }

    // 指定されたオプションの数をカウント
    int     optionCount = 0;
    QString specifiedOption;

    if (parser.isVersionSet()) {
        optionCount++;
        specifiedOption = "version";
    }

    if (parser.isHelpSet()) {
        optionCount++;
        specifiedOption = "help";
    }

    if (parser.isSysConfSet()) {
        optionCount++;
        specifiedOption = "sysconf";
    }

    const QStringList unknownOptions = parser.unknownOptionNames();
    if (!unknownOptions.isEmpty()) {
        optionCount += unknownOptions.size();
        specifiedOption = unknownOptions.first();
    }

    if (optionCount > 1) {
        std::cerr << QString("エラー : 指定できるオプションは1つのみです").toStdString() << std::endl;
        QCoreApplication::exit();
        return;
    }
    else if (optionCount == 0) {
        std::cerr << QString("エラー : オプションがありません").toStdString() << std::endl;
        QCoreApplication::exit();
        return;
    }

    if (parser.isSet(versionOption)) {
        // --version / -vオプション
        auto version = QString("qNewsFlash %1.%2.%3\n").arg(PROJECT_VERSION_MAJOR).arg(PROJECT_VERSION_MINOR).arg(PROJECT_VERSION_PATCH)
                       + QString("ライセンス : UNLICENSE\n")
                       + QString("ライセンスの詳細な情報は、<http://unlicense.org/> を参照してください\n\n")
                       + QString("開発者 : Presire with ﾘ* ﾞㇷﾞ)ﾚ の みんな\n\n");
        std::cout << version.toStdString() << std::endl;

        QCoreApplication::exit();
        return;
    }
    else if (parser.isSet(helpOption)) {
        // --help / -h オプション
        auto help = QString("使用法 : qNewsFlash [オプション]\n\n")
                    + QString("\t--sysconf=<qNewsFlash.jsonファイルのパス>\t設定ファイルのパスを指定する\n")
                    + QString("\t-v, --version\t\t\t\tバージョン情報を表示する\n\n");
        std::cout << help.toStdString() << std::endl;

        QCoreApplication::exit();
        return;
    }
    else if (parser.isSet(sysconfOption)) {
        // --sysconfオプションの値を取得
        auto option = parser.value(sysconfOption);

        // 先頭と末尾にクォーテーションが存在する場合は取り除く
        if ((option.startsWith('\"') && option.endsWith('\"')) || (option.startsWith('\'') && option.endsWith('\''))) {
            option = option.mid(1, option.length() - 2);
        }

        if (option.isEmpty()) {
            std::cerr << QString("エラー : 設定ファイルのパスが不明です").toStdString() << std::endl;

            QCoreApplication::exit();
            return;
        }

        m_SysConfFile = option;

        if (getConfiguration(m_SysConfFile)) {
            QCoreApplication::exit();
            return;
        }
    }
    else {
        std::cerr << QString("エラー : 不明なオプションです - %1").arg(parser.isSet(specifiedOption)).toStdString() << std::endl;

        QCoreApplication::exit();
        return;
    }

    // ログファイルの設定
    if (checkLogFile(m_LogFile)) {
        QCoreApplication::exit();
        return;
    }

    m_pWriteMode = WriteMode::getInstance();
    m_pWriteMode->setSysConfFile(m_SysConfFile);    // qNewsFlashの設定ファイルを指定
    m_pWriteMode->setLogFile(m_LogFile);            // スレッドに書き込み済みのニュース記事を保存するJSONファイルのパスを指定

    // ログファイルから、昨日以前(昨日も含む)の書き込み済みのニュース記事を削除
    if (m_pWriteMode->deleteLogNotToday()) {
        QCoreApplication::exit();
        return;
    }

    // ログファイルから、今日と昨日の書き込み済みのニュース記事を取得
    // また、取得した記事群のデータは、メンバ変数m_WrittenArticlesに保存
    try {
        m_WrittenArticles = m_pWriteMode->getDatafromWrittenLog();
    }
    catch (const std::runtime_error &e) {
        // ログファイルのオープンや読み込みに失敗した場合
        std::cerr << QString("%1").arg(e.what()).toStdString();

        QCoreApplication::exit();
        return;
    }
    catch (const std::exception &e) {
        // その他の例外をキャッチ
        std::cerr << QString("%1").arg(e.what()).toStdString();

        QCoreApplication::exit();
        return;
    }

    // !bottomコマンドが有効な場合、
    // ログファイルから指定時間が経っている該当オブジェクトの"thread"オブジェクト -> "bottom"キーを"true"に更新
    if (m_WriteInfo.BottomThread) {
        if (m_pWriteMode->writeBottomLogInitialization(m_ThreadInfo, m_WriteInfo, m_Bottominterval)) {
            QCoreApplication::exit();
            return;
        }
    }

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
    // JSONファイル(スレッド書き込み用)のパスが空の場合は、デフォルトのパスを使用
    if (m_WriteFile.isEmpty()) {
        m_WriteFile = QString("/tmp/qNewsFlashWrite.json");
    }
#endif

    if (m_AutoFetch) {
        // ニュース記事の自動取得タイマの開始
        connect(&m_timer, &QTimer::timeout, this, &Runner::fetchNonBreakingNews);
        m_timer.start(static_cast<int>(m_interval));

        // (時事ドットコム) 速報記事の自動取得タイマの開始
        if (m_bJiJiFlash) {
            connect(&m_JiJiTimer, &QTimer::timeout, this, &Runner::JiJiFlashfetch);
            m_JiJiTimer.start(static_cast<int>(m_JiJiinterval));
        }

        // (共同通信) 速報記事の自動取得タイマの開始
        if (m_bKyodoFlash) {
            connect(&m_KyodoTimer, &QTimer::timeout, this, &Runner::KyodoFlashfetch);
            m_KyodoTimer.start(static_cast<int>(m_Kyodointerval));
        }

        // !bottomコマンドの書き込みタイマの開始
        if (m_WriteInfo.BottomThread) {
            connect(&m_BottomTimer, &QTimer::timeout, this, &Runner::bottomThread);
            m_BottomTimer.start(static_cast<int>(m_Bottominterval));
        }
    }

    // 本ソフトウェア開始直後に各ニュース記事を読み込む場合は、コメントを解除して、fetchNonBreakingNews()メソッドを実行する
    // コメントアウトしている場合、かつ、通常実行またはSystemdサービスで実行する場合、最初に各ニュース記事を読み込むタイミングは、タイマの指定時間後となる
    fetchNonBreakingNews();

    // 本ソフトウェア開始直後に時事ドットコムから速報記事を読み込む場合は、コメントを解除して、JiJiFlashfetch()メソッドを実行する
    // コメントアウトしている場合、かつ、通常実行またはSystemdサービスで実行する場合、最初に速報記事を読み込むタイミングは、タイマの指定時間後となる
    JiJiFlashfetch();

    // 本ソフトウェア開始直後に共同通信から速報記事を読み込む場合は、コメントを解除して、KyodoFlashfetch()メソッドを実行する
    // コメントアウトしている場合、かつ、通常実行またはSystemdサービスで実行する場合、最初に速報記事を読み込むタイミングは、タイマの指定時間後となる
    KyodoFlashfetch();

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


// 速報ニュース以外のニュース記事の取得する
void Runner::fetchNonBreakingNews()
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
        if (m_pWriteMode->deleteLogNotToday()) {
            QCoreApplication::exit();
            return;
        }

        /// ログファイルから、今日と昨日の書き込み済みのニュース記事を取得
        /// また、取得した記事群のデータはメンバ変数m_WrittenArticlesに格納
        try {
            m_WrittenArticles = m_pWriteMode->getDatafromWrittenLog();
        }
        catch (const std::runtime_error &e) {
            // ログファイルのオープンや読み込みに失敗した場合
            std::cerr << QString("%1").arg(e.what()).toStdString();

            QCoreApplication::exit();
            return;
        }
        catch (const std::exception &e) {
            // その他の例外をキャッチ
            std::cerr << QString("%1").arg(e.what()).toStdString();

            QCoreApplication::exit();
            return;
        }
    }

    // 設定ファイルの"update"キーを更新
    if (m_pWriteMode->updateDateJson(m_LastUpdate)) {
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

    // 毎日新聞の記事を取得
    if (m_bMainichi) {
        // 毎日新聞のRSSフィードのURLを指定
        QUrl urlMainichi(m_MainichiRSS);

        /// HTTPリクエストを作成して、ヘッダを設定
        QNetworkRequest requestMainichi(urlMainichi);

        /// HTTPリクエストを送信
        m_pReplyMainichi = manager->get(requestMainichi);

        /// HTTPレスポンスを受信した後、Runner::fetchMainichiRSS()メソッドを実行
        QObject::connect(m_pReplyMainichi, &QNetworkReply::finished, this, &Runner::fetchMainichiRSS);

        /// レスポンス待機
        QEventLoop loop;
        QObject::connect(this, &Runner::Mainichifinished, &loop, &QEventLoop::quit);
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

        // 書き込みモードの設定
        m_pWriteMode->setArticle(article);              // 書き込むニュース記事を指定
        m_pWriteMode->setThreadInfo(m_ThreadInfo);      // スレッド情報に関する設定を指定
        m_pWriteMode->setWriteInfo(m_WriteInfo);        // 書き込み情報に関する設定を指定

        // ニュース記事の書き込み
        if (m_WriteMode == 1) {
            // 書き込みモード 1 : 1つのスレッドにニュース記事および速報ニュースを書き込むモード
            auto iRet = m_pWriteMode->writeMode1();
            if (iRet == WriteMode::WRITEERROR::POSTERROR) {
                return;
            }
            else if (iRet == WriteMode::WRITEERROR::LOGERROR) {
                QCoreApplication::exit();
                return;
            }
        }
        else if (m_WriteMode == 2 || m_WriteMode == 3) {
            // 書き込みモード 2 : ニュース記事および速報ニュースにおいて、常に新規スレッドを立てるモード
            // 書き込みモード 3 : 速報ニュース以外は、常に新規スレッドを立てるモード
            auto iRet = m_pWriteMode->writeMode2();
            if (iRet == WriteMode::WRITEERROR::POSTERROR) {
                return;
            }
            else if (iRet == WriteMode::WRITEERROR::LOGERROR) {
                QCoreApplication::exit();
                return;
            }
        }
        else {
            std::cerr << QString("エラー : 不明な書き込みモード \"%1\"").arg(m_WriteMode).toStdString() << std::endl;
            QCoreApplication::exit();
            return;
        }

        // ニュース記事を書き込むスレッドの情報を更新
        m_ThreadInfo = m_pWriteMode->getThreadInfo();

        // スレッドの書き込みに関する情報を更新
        m_WriteInfo  = m_pWriteMode->getWriteInfo();

        // 書き込み済みの記事を履歴として登録 (同じ記事を1日に2回以上書き込まないようにする)
        // ただし、2日前以上の書き込み済み記事の履歴は削除する
        m_WrittenArticles.append(article);
    }
#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
    // qNewsFlash 0.1.0未満の機能
    else {

        // スレッド書き込み用のJSONファイルの内容を空にする
        if (m_pWriteMode->truncateJSON()) {
            QCoreApplication::exit();
            return;
        }
    }
#endif

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;
}


// News APIからニュース記事の取得後に実行する
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


// 時事ドットコムからニュース記事の取得後に実行する
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


// 時事ドットコムのニュース記事(RSS)を分解して取得する
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


// 共同通信からニュース記事の取得後に実行する
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


// 共同通信のニュース記事(RSS)を分解して取得する
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
                        // QTextDocumentFragmentクラスはQtGuiモジュールが必要となるため、
                        // 代替として、Qtcoreモジュールのみで使用できるQXmlStreamReaderクラスを使用する
                        //paragraph = QTextDocumentFragment::fromHtml(paragraph).toPlainText();
                        QXmlStreamReader xml(paragraph);
                        QString result = "";

                        while (!xml.atEnd()) {
                            if (xml.readNext() == QXmlStreamReader::Characters) {
                                result += xml.text();
                            }
                        }
                        paragraph = result.trimmed();

                        // 本文が指定文字数以上の場合、指定文字数分のみを抽出
                        paragraph = paragraph.size() > m_MaxParagraph ? paragraph.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : paragraph;
                    }
                    else if (xmlStrcmp(itemChild->name, BAD_CAST "link") == 0) {
                        link = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        if (m_KyodoNewsOnly) {
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


// 朝日新聞デジタルからニュース記事の取得後に実行する
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


// 朝日新聞デジタルのニュース記事(RSS)を分解して取得する
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


// 毎日新聞からニュース記事の取得後に実行する
void Runner::fetchMainichiRSS()
{
    auto byteArray  = m_pReplyMainichi->readAll();
    auto xmlContent = byteArray.constData();

    // libxml2の初期化
    xmlInitParser();
    LIBXML_TEST_VERSION

    // メモリバッファからXMLをパース
    auto *doc = xmlReadMemory(xmlContent, byteArray.size(), "noname.xml", nullptr, 0);
    if (doc == nullptr) {
        std::cerr << "Failed to parse XML from memory" << std::endl;
        emit Mainichifinished();

        return;
    }

    // ルート要素を取得
    auto *root_element = xmlDocGetRootElement(doc);

    // 各itemタグを処理
    itemTagsforMainichi(root_element);

    // ドキュメントを解放
    xmlFreeDoc(doc);

    // libxml2をクリーンアップ
    xmlCleanupParser();

    m_pReplyMainichi->deleteLater();

    emit Mainichifinished();
}


// 毎日新聞のニュース記事(RSS)を分解して取得する
void Runner::itemTagsforMainichi(xmlNode *a_node)
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

                        if (fetcher.fetch(url, true, m_MainichiParaXPath)) {
                            // 本文の取得に失敗した場合
                            bSkipNews = true;
                            break;
                        }

                        // ニュース記事の概要を取得
                        // 本文の先頭および最後尾に空白が入ることがあるため消去
                        paragraph = fetcher.getParagraph().trimmed();
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
        itemTagsforMainichi(cur_node->children);
    }
}


// CNET Japanからニュース記事の取得後に実行する
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


// CNET Japanのニュース記事(RSS)を分解して取得する
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
                        // paragraph = QString::fromUtf8(reinterpret_cast<const char*>(xmlNodeGetContent(itemChild)));

                        // // 不要なhtmlタグを除去
                        // static QRegularExpression re2("<br .*</a>", QRegularExpression::DotMatchesEverythingOption);
                        // paragraph.remove(re2);

                        // // 不要な文字を削除 (\n, \t)
                        // static QRegularExpression re1("[\t\n]", QRegularExpression::CaseInsensitiveOption);
                        // paragraph = paragraph.replace(re1, "");

                        // // 不要な文字を削除 (スペース等)
                        // static QRegularExpression re3("[\\s]", QRegularExpression::CaseInsensitiveOption);
                        // paragraph = paragraph.replace(re3, "").replace(" ", "").replace("\u3000", "");

                        // // 本文が指定文字数以上の場合、指定文字数分のみを抽出
                        // paragraph = paragraph.size() > m_MaxParagraph ? paragraph.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : paragraph;
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


// ハンギョレジャパンからニュース記事の取得後に実行する
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


// ハンギョレジャパンのニュース記事(RSS)を分解して取得する
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
                        // QUrl url(link);
                        // HtmlFetcher fetcher(m_MaxParagraph, this);

                        // if (fetcher.fetch(url, true, QString("//head/meta[@property='og:description']/@content"))) {
                        //     // 本文の取得に失敗した場合
                        //     bSkipNews = true;
                        //     break;
                        // }

                        // paragraph = fetcher.getParagraph();
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


// ロイター通信からニュース記事の取得後に実行する
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


// ロイター通信のニュース記事(RSS)を分解して取得
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


// 東京新聞からニュース記事の取得後に実行する
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


// 時事ドットコムから速報記事の取得する
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
        if (m_pWriteMode->deleteLogNotToday()) {
            QCoreApplication::exit();
            return;
        }

        /// ログファイルから、今日と昨日の書き込み済みのニュース記事を取得
        /// また、取得した記事群のデータはメンバ変数m_WrittenArticlesに格納
        try {
            m_WrittenArticles = m_pWriteMode->getDatafromWrittenLog();
        }
        catch (const std::runtime_error &e) {
            // ログファイルのオープンや読み込みに失敗した場合
            std::cerr << QString("%1").arg(e.what()).toStdString();

            QCoreApplication::exit();
            return;
        }
        catch (const std::exception &e) {
            // その他の例外をキャッチ
            std::cerr << QString("%1").arg(e.what()).toStdString();

            QCoreApplication::exit();
            return;
        }
    }

    // 設定ファイルの"update"キーを更新
    if (m_pWriteMode->updateDateJson(m_LastUpdate)) {
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
    // (現在は使用しない)
    // auto isCheckDate = isToday(pubDate);
    // if (!isCheckDate) {
    //     return;
    // }

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

    // 書き込みモードの設定
    m_pWriteMode->setArticle(title, "", link, pubDate);     // 書き込むニュース記事を指定
    m_pWriteMode->setThreadInfo(m_ThreadInfo);              // スレッド情報に関する設定を指定
    m_pWriteMode->setWriteInfo(m_WriteInfo);                // 書き込み情報に関する設定を指定

    // ニュース記事の書き込み
    if (m_WriteMode == 1 || m_WriteMode == 3) {
        // 書き込みモード 1, 3 : 1つのスレッドにニュース記事および速報ニュースを書き込むモード
        auto iRet = m_pWriteMode->writeMode1();
        if (iRet == WriteMode::WRITEERROR::POSTERROR) {
            return;
        }
        else if (iRet == WriteMode::WRITEERROR::LOGERROR) {
            QCoreApplication::exit();
            return;
        }
    }
    else if (m_WriteMode == 2) {
        // 書き込みモード 2 : ニュース記事および速報ニュースにおいて、常に新規スレッドを立てるモード
        auto iRet = m_pWriteMode->writeMode2();
        if (iRet == WriteMode::WRITEERROR::POSTERROR) {
            return;
        }
        else if (iRet == WriteMode::WRITEERROR::LOGERROR) {
            QCoreApplication::exit();
            return;
        }
    }
    else {
        std::cerr << QString("エラー : 不明な書き込みモード \"%1\"").arg(m_WriteMode).toStdString() << std::endl;
        QCoreApplication::exit();
        return;
    }

    // ニュース記事を書き込むスレッドの情報を更新
    m_ThreadInfo = m_pWriteMode->getThreadInfo();

    // スレッドの書き込みに関する情報を更新
    m_WriteInfo  = m_pWriteMode->getWriteInfo();

    // 書き込み済みの記事を履歴として登録 (同じ記事を1日に2回以上書き込まないようにする)
    // ただし、2日前以上の書き込み済み記事の履歴は削除する
    Article article(title, paragraph, link, pubDate);
    m_WrittenArticles.append(article);

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;

    return;
}


// 共同通信から速報記事の取得するスロット
void Runner::KyodoFlashfetch()
{
    // 時事ドットコムの速報記事を取得するかどうかを確認
    if (!m_bKyodoFlash) {
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
        if (m_pWriteMode->deleteLogNotToday()) {
            QCoreApplication::exit();
            return;
        }

        /// ログファイルから、今日と昨日の書き込み済みのニュース記事を取得
        /// また、取得した記事群のデータはメンバ変数m_WrittenArticlesに格納
        try {
            m_WrittenArticles = m_pWriteMode->getDatafromWrittenLog();
        }
        catch (const std::runtime_error &e) {
            // ログファイルのオープンや読み込みに失敗した場合
            std::cerr << QString("%1").arg(e.what()).toStdString();

            QCoreApplication::exit();
            return;
        }
        catch (const std::exception &e) {
            // その他の例外をキャッチ
            std::cerr << QString("%1").arg(e.what()).toStdString();

            QCoreApplication::exit();
            return;
        }
    }

    // 設定ファイルの"update"キーを更新
    if (m_pWriteMode->updateDateJson(m_LastUpdate)) {
        QCoreApplication::exit();
        return;
    }

    // 前回取得した書き込み前の記事群(選定前)を初期化
    m_BeforeWritingArticles.clear();

    if (m_stopRequested.load()) return;

    // 共同通信から速報記事の取得
    KyodoFlash kyodoFlash(m_MaxParagraph, m_KyodoFlashInfo, this);
    if (kyodoFlash.FetchFlash()) {
        return;
    }

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;

    auto [title, paragraph, link, pubDate] = kyodoFlash.getArticleData();

    // 速報記事の公開日を確認
    // 今日の速報記事ではない場合は無視
    // (現在は使用しない)
    // auto isCheckDate = isToday(pubDate);
    // if (!isCheckDate) {
    //     return;
    // }

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
    qDebug() << "Title : "      << title;
    qDebug() << "Paragraph : "  << paragraph;
    qDebug() << "URL : "        << link;
    qDebug() << "Date : "       << pubDate;
    qDebug() << "";
#endif

    // 書き込みモードの設定
    m_pWriteMode->setArticle(title, paragraph.isEmpty() ? "" : paragraph, link, pubDate);  // 書き込むニュース記事を指定
    m_pWriteMode->setThreadInfo(m_ThreadInfo);                                             // スレッド情報に関する設定を指定
    m_pWriteMode->setWriteInfo(m_WriteInfo);                                               // 書き込み情報に関する設定を指定

    // ニュース記事の書き込み
    if (m_WriteMode == 1 || m_WriteMode == 3) {
        // 書き込みモード 1, 3 : 1つのスレッドにニュース記事および速報ニュースを書き込むモード
        auto iRet = m_pWriteMode->writeMode1();
        if (iRet == WriteMode::WRITEERROR::POSTERROR) {
            return;
        }
        else if (iRet == WriteMode::WRITEERROR::LOGERROR) {
            QCoreApplication::exit();
            return;
        }
    }
    else if (m_WriteMode == 2) {
        // 書き込みモード 2 : ニュース記事および速報ニュースにおいて、常に新規スレッドを立てるモード
        auto iRet = m_pWriteMode->writeMode2();
        if (iRet == WriteMode::WRITEERROR::POSTERROR) {
            return;
        }
        else if (iRet == WriteMode::WRITEERROR::LOGERROR) {
            QCoreApplication::exit();
            return;
        }
    }
    else {
        std::cerr << QString("エラー : 不明な書き込みモード \"%1\"").arg(m_WriteMode).toStdString() << std::endl;
        QCoreApplication::exit();
        return;
    }

    // ニュース記事を書き込むスレッドの情報を更新
    m_ThreadInfo = m_pWriteMode->getThreadInfo();

    // スレッドの書き込みに関する情報を更新
    m_WriteInfo  = m_pWriteMode->getWriteInfo();

    // 書き込み済みの記事を履歴として登録 (同じ記事を1日に2回以上書き込まないようにする)
    // ただし、2日前以上の書き込み済み記事の履歴は削除する
    Article article(title, paragraph, link, pubDate);
    m_WrittenArticles.append(article);

    // [q]キーまたは[Q]キー ==> [Enter]キーが押下されている場合は終了
    if (m_stopRequested.load()) return;

    return;
}


// UTC時刻から日本時間および"yyyy/M/d h時m分"に変換する (News API等で使用)
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


// ISO8601形式の時刻を"yyyy年M月d日 H時m分"に変換 (時事ドットコム、ロイター通信等で使用)
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


// RFC 2822形式の時刻を"yyyy年M月d日 H時m分"に変換 (ハンギョレジャパン等で使用)
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


// ニュース記事が今日の日付かどうかを確認
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


// ニュース記事が指定時間以内の時刻かどうかを確認
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

    // N時間前の日時を計算
    QDateTime hoursAgo = todayTime.addSecs(- m_WithinHours * 60 * 60);

    // ニュース記事の公開日時が現在日時の指定時間以内にあるかどうかを確認
    bool isInTargetDateTime = (convertPubDate >= hoursAgo && convertPubDate <= todayTime);

    return isInTargetDateTime;
}


// 本ソフトウェアの設定ファイルの情報を取得
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
        std::cerr << QString("エラー : 設定ファイルが存在しません (%1)").arg(filepath).toStdString() << std::endl;
        return -1;
    }

    // ロックファイルの生成
    QFileInfo configFileInfo(filepath);
    QString   lockFilePath = configFileInfo.dir().filePath(configFileInfo.baseName() + ".lock");
    QLockFile lockFile(lockFilePath);

    // 設定ファイルを読み込む
    QFile File(filepath);

    try {
        // 設定ファイルを開く
        if(!File.open(QIODevice::ReadOnly | QIODevice::Text)) {
            throw std::runtime_error(QString("設定ファイルのオープンに失敗しました %1").arg(File.errorString()).toStdString());
        }

        // 設定ファイルを読み込む
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

        // News APIを有効にしている場合、かつ、News APIのキーが空の場合はエラー
        if (m_bNewsAPI && m_API.isEmpty()) {
            throw std::runtime_error("News APIが有効化されていますが、News APIのキーが設定されていません");
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
                    throw std::runtime_error("インターバルの値が不正です");
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
        m_KyodoNewsOnly     = KyodoObject["newsonly"].toBool(true);   /// 共同通信から取得するニュース記事の種類をニュースのみに絞るかどうか
                                                                      /// ニュース以外の記事では、ビジネス関連やライフスタイル等の記事がある

        // 共同通信の速報ニュース記事
        auto kyodoFlashObject  = JsonObject["kyodoflash"].toObject();     /// 共同通信の速報ニュースオブジェクトの取得
        m_bKyodoFlash          = kyodoFlashObject["enable"].toBool(false);  /// 共同通信の速報ニュースの有効 / 無効
        if (m_bKyodoFlash) {
            /// 共同通信の速報ニュースを取得する時間間隔 (未指定の場合、インターバルは10[分])
            /// ただし、1分未満には設定できない (1分未満に設定した場合は、1分に設定する)
            auto kyodoInterval = kyodoFlashObject["interval"].toString("600");
            bool ok;
            m_Kyodointerval = kyodoInterval.toULongLong(&ok);
            if (!ok) {
                std::cerr << QString("警告 : 設定ファイルのkyodoflash:intervalキーの値が不正です").toStdString() << std::endl;
                std::cerr << QString("更新時間の間隔は、自動的に10分に設定されます").toStdString() << std::endl;

                m_Kyodointerval = 10 * 60 * 1000;
            }
            else {
                if (m_Kyodointerval == 0) {
                    std::cerr << "インターバルの値が未指定もしくは0のため、10[分]に設定されます" << std::endl;
                    m_Kyodointerval = 10 * 60 * 1000;
                }
                else if (m_Kyodointerval < (1 * 60)) {
                    std::cerr << "インターバルの値が1[分]未満のため、1[分]に設定されます" << std::endl;
                    m_Kyodointerval = 1 * 60 * 1000;
                }
                else if (m_Kyodointerval < 0) {
                    throw std::runtime_error("インターバルの値が不正です");
                }
                else {
                    m_Kyodointerval *= 1000;
                }
            }

            /// 共同通信の速報記事の基準となるURL
            m_KyodoFlashInfo.BasisURL    = kyodoFlashObject["basisurl"].toString("");

            /// 共同通信の速報記事の一覧が存在するURL
            m_KyodoFlashInfo.FlashUrl     = kyodoFlashObject["flashurl"].toString("");

            /// 共同通信の速報記事が記載されているURLから速報記事のリンクを取得するXPath
            m_KyodoFlashInfo.FlashXPath   = kyodoFlashObject["flashxpath"].toString("");

            /// 共同通信の各速報記事のURLから記事のタイトルを取得するXPath
            m_KyodoFlashInfo.TitleXPath   = kyodoFlashObject["titlexpath"].toString("");

            /// 共同通信の各速報記事のURLから記事の本文を取得するXPath
            m_KyodoFlashInfo.ParaXPath    = kyodoFlashObject["paraxpath"].toString("");

            /// 共同通信の各速報記事のURLから記事の公開日を取得するXPath
            m_KyodoFlashInfo.PubDateXPath = kyodoFlashObject["pubdatexpath"].toString("");
        }

        // 朝日新聞デジタルのニュース記事
        auto AsahiObject    = JsonObject["asahi"].toObject();         /// 朝日新聞デジタルのニュースオブジェクトの取得
        m_bAsahi            = AsahiObject["enable"].toBool(false);    /// 朝日新聞デジタルの有効 / 無効
        m_AsahiRSS          = AsahiObject["rss"].toString("");        /// 朝日新聞デジタルからニュース記事を取得するためのRSS (URL)

        // 毎日新聞のニュース記事
        auto MainichiObject  = JsonObject["mainichi"].toObject();         /// 毎日新聞のニュースオブジェクトの取得
        m_bMainichi          = MainichiObject["enable"].toBool(false);    /// 毎日新聞の有効 / 無効
        m_MainichiRSS        = MainichiObject["rss"].toString("");        /// 毎日新聞からニュース記事を取得するためのRSS (URL)
        m_MainichiParaXPath  = MainichiObject["paraxpath"].toString("");  /// 毎日新聞からニュース記事の概要を取得するためのXPath式
        if (m_bMainichi && (m_MainichiRSS.isEmpty() || m_MainichiParaXPath.isEmpty())) {
            std::cerr << QString("エラー : 毎日新聞が有効になっていますが、\"rss\"もしくは\"paraxpath\"の値が設定されていません").toStdString() << std::endl;
        }

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
        m_WriteInfo.RequestURL            = JsonObject["requesturl"].toString("");

        // スレッド情報
        m_ThreadInfo.subject    = JsonObject["subject"].toString("");
        m_ThreadInfo.from       = JsonObject["from"].toString("");
        m_ThreadInfo.mail       = JsonObject["mail"].toString("");      // メール欄
        m_ThreadInfo.bbs        = JsonObject["bbs"].toString("");       // BBS名
        m_ThreadInfo.key        = JsonObject["key"].toString("");       // スレッド番号
        m_ThreadInfo.shiftjis   = JsonObject["shiftjis"].toBool(true);  // Shift-JISの有効 / 無効

        // スレッドのタイトルを変更するかどうか
        // この機能は、防弾嫌儲およびニュース速報(Libre)等のスレッドのタイトルが変更できる掲示板で使用可能
        m_WriteInfo.ChangeTitle = JsonObject["chtt"].toBool(false);
#elif QNEWSFLASH_VERSION_MAJOR > 0 || (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR >= 3)
        // qNewsFlash 0.3.0以降

        // スレッド情報
        auto threadObject  = JsonObject["thread"].toObject();

        /// 書き込みモード
        /// 書き込みモード 1 : 1つのスレッドにニュース記事を書き込むモード
        /// 書き込みモード 2 : ニュース記事ごとに新規スレッドを立てるモード
        /// 書き込みモード 3 : 速報記事は1つのスレッドに書き込み、それ以外のニュース記事はニュース記事ごとに新規スレッドを立てるモード
        auto WriteMode  = threadObject["writemode"].toInt(1);
        switch (WriteMode) {
            case 1:
            case 2:
            case 3:
                m_WriteMode = WriteMode;
                break;
            [[fallthrough]];
            default:
                throw std::runtime_error(QString("不明な書き込みモードです (書き込みモード %1)").arg(WriteMode).toStdString());
                break;
        }

        /// 掲示板へPOSTデータを送信するURL
        m_WriteInfo.RequestURL       = threadObject["requesturl"].toString("");
        if (m_WriteInfo.RequestURL.isEmpty()) {
            throw std::runtime_error("掲示板へPOSTデータを送信するURLが空欄です");
        }

        /// 書き込むスレッドのURL
        /// 設定ファイル内のスレッドのURLは、書き込みモード 1 および 書き込みモード 3の速報ニュースのみで使用
        m_WriteInfo.ThreadURL        = threadObject["threadurl"].toString("");
        if (m_WriteInfo.ThreadURL.isEmpty()) {
            if      (WriteMode == 1)                                    std::cout << QString("スレッドのURLが空欄のため、専用スレッドを新規作成します").toStdString() << std::endl;
            else if (WriteMode == 3 && (m_bJiJiFlash || m_bKyodoFlash)) std::cout << QString("スレッドのURLが空欄のため、速報ニュースは専用スレッドを新規作成します").toStdString() << std::endl;
        }

        /// 書き込み済みのスレッドのタイトル
        m_WriteInfo.ThreadTitle      = threadObject["threadtitle"].toString("");

        /// スレッドのレス数を取得するためのXPath
        /// デフォルト値は、0ch系掲示板のデフォルトのXPath式
        m_WriteInfo.ThreadXPath      = threadObject["threadxpath"].toString("/html/body//div/dl[@class='thread']//div/@id");
        if (m_WriteInfo.ThreadXPath.isEmpty()) {
            throw std::runtime_error("スレッドのレス数を取得するためのXPathが空欄です");
        }

        /// スレッドの最大レス数
        m_WriteInfo.MaxThreadNum     = threadObject["max"].toInt(1000);

        /// POSTデータに関する情報
        auto subject                 = threadObject["subject"].toString("");   // スレッドのタイトル (スレッドを新規作成する場合)
                                                                               // 空欄の場合、スレッドのタイトルは取得したニュース記事のタイトルとなる
        /// エスケープされた%tトークンを一時的に保護
        QString tempToken = "__TEMP__";
        subject.replace("%%t", tempToken);

        if (subject.count("%t") > 1) {
            /// %tトークンが複数存在する場合はエラー
            throw std::runtime_error("\"%t\"トークンが複数存在します");
        }
        else {
            /// 保護されたトークンを元に戻す
            subject.replace(tempToken, "%%t");
            m_ThreadInfo.subject = subject;
        }

        m_ThreadInfo.from       = threadObject["from"].toString("");      // 名前欄
        m_ThreadInfo.mail       = threadObject["mail"].toString("");      // メール欄
        m_ThreadInfo.bbs        = threadObject["bbs"].toString("");       // BBS名
        m_ThreadInfo.key        = threadObject["key"].toString("");       // スレッド番号 (書き込みモード 1 および 書き込みモード 3の速報ニュースのみで使用)
        m_ThreadInfo.shiftjis   = threadObject["shiftjis"].toBool(true);  // Shift-JISの有効 / 無効

        // Webページから一意のタグの値を取得
        // この値を確認して、スレッドが落ちているかどうかを判断する
        // デフォルトの設定では、スレッドが落ちている状態のスレッドタイトル名
        m_WriteInfo.ExpiredElement     = threadObject["expiredelement"].toString("");

        // スレッドが落ちた時のスレッドタイトル名を取得するXPath
        m_WriteInfo.ExpiredXpath       = threadObject["expiredxpath"].toString("");

        // 新規スレッドのタイトル名とスレッドが落ちた時のスレッドタイトル名が同じ場合はエラー
        //if (m_ThreadInfo.subject.compare(m_WriteInfo.ExpiredElement, Qt::CaseSensitive) == 0) {
        //    throw std::runtime_error("新規スレッドのタイトル名とスレッドが落ちている状態のエレメント名には、別の文言を指定してください");
        //}

        // スレッドコマンド
        auto threadCommandObject    = JsonObject["threadcommand"].toObject();

        /// スレッドのタイトルを変更するかどうか
        /// この機能は、防弾嫌儲およびニュース速報(Libre)等のスレッドのタイトルが変更できる掲示板で使用可能
        m_WriteInfo.ChangeTitle     = threadCommandObject["chtt"].toBool(false);

        /// 一定時間を超えてレスが無いスレッドをsage進行にするかどうか
        /// ニュース速報(Libre)等の掲示板でのみ使用可能
        auto bottomObject           = threadCommandObject["bottom"].toObject();
        m_WriteInfo.BottomThread    = bottomObject["enable"].toBool(false);
        if (m_WriteInfo.BottomThread) {
            // デフォルトは180[分] = 3[時間]
            auto bottomInterval     = bottomObject["interval"].toString("180");
            bool ok;
            m_Bottominterval = bottomInterval.toULongLong(&ok);
            if (!ok) {
                std::cerr << QString("警告 : 設定ファイルのthreadcommand:bottom:intervalキーの値が不正です").toStdString() << std::endl;
                std::cerr << QString("更新時間の間隔は、自動的に180分に設定されます").toStdString() << std::endl;

                m_Bottominterval = 180 * 60 * 1000;
            }
            else {
                if (m_Bottominterval == 0) {
                    std::cerr << "!bottomコマンドのインターバルの値が未指定もしくは0のため、180[分]に設定されます" << std::endl;
                    m_Bottominterval = 180 * 60 * 1000;
                }
                else if (m_Bottominterval > 1440) {
                    std::cerr << "!bottomコマンドのインターバルの値が1440[分]を超えるため、180[分]に設定されます" << std::endl;
                    m_Bottominterval = 180 * 60 * 1000;
                }
                else if (m_Bottominterval < 0) {
                    throw std::runtime_error("!bottomコマンドのインターバルの値が不正です");
                }
                else {
                    m_Bottominterval *= 60 * 1000;
                }
            }
        }

        /// スレッドが5日でdat落ちする基準を変更するかどうか
        /// 防弾嫌儲およびニュース速報(Libre)等の掲示板でのみ使用可能
        m_WriteInfo.SaveThread      = threadCommandObject["hogo"].toBool(false);
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


// 取得したニュース記事群からランダムで1つを選択
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


// このソフトウェアのログ情報を保存するファイルのパスを設定
int Runner::checkLogFile(QString &filepath)
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
int Runner::checkLogFile()
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


// 書き込み済みのスレッドに!bottomコマンドを書き込むスロット
void Runner::bottomThread()
{
    if (m_WriteInfo.BottomThread) {
        if (m_AutoFetch) {
            // 自動取得機能を有効にしている場合

            // タイマを一時停止
            m_BottomTimer.stop();

            // 最初にスレッドに書き込んだ日時を取得
            auto date = m_pWriteMode->getOldestWriteLogDate();
            if (!date.isEmpty()) {
                // 書き込み済みのスレッドにレスが無い場合は、該当スレッドに!bottomコマンドを書き込む
                if (m_pWriteMode->writeBottom()) {
                    std::cerr << QString("エラー: !bottomコマンドの書き込みに失敗").toStdString() << std::endl;
                }

                // !bottomコマンドを書き込む予定のスレッドに対して、次回のインターバルを指定
                auto nextDate = m_pWriteMode->getOldestWriteLogDate();
                if (nextDate.isEmpty()) {
                    m_BottomTimer.start(static_cast<int>(m_Bottominterval));
                    return;
                }

                /// QDateTimeに変換
                QDateTime nextDateTime = QDateTime::fromString(nextDate, "yyyy年M月d日 h時m分");
                if (!nextDateTime.isValid()) {
                    std::cerr << QString("エラー: !bottomコマンド機能で使用する次回の時刻が無効です").toStdString() << std::endl;
                    std::cerr << QString("設定ファイルのインターバル %1[分] を使用します").arg(m_Bottominterval / 60 / 1000).toStdString() << std::endl;
                    m_BottomTimer.start(static_cast<int>(m_Bottominterval));

                    return;
                }

                /// 日本のタイムゾーンを設定
                QTimeZone japanTimeZone("Asia/Tokyo");

                /// 現在の日本時間を取得
                QDateTime japanTime = QDateTime::currentDateTime().toTimeZone(japanTimeZone);

                /// 日本語ロケールを設定
                QLocale japaneseLocale(QLocale::Japanese, QLocale::Japan);

                /// 指定された形式で日時を文字列に変換
                QString formattedTime = japaneseLocale.toString(japanTime, "yyyy年M月d日 H時m分");

                /// QDateTimeオブジェクトを生成
                QDateTime currentDateTime = japaneseLocale.toDateTime(formattedTime, "yyyy年M月d日 H時m分");

                /// 差分をミリ秒単位で計算
                qint64 difference = nextDateTime.msecsTo(currentDateTime);

                /// 次回のインターバルを求める
                auto nextInterval = m_Bottominterval - difference;

                // タイマを再開 (開始時刻を更新)
                if (nextInterval > 0) {
                    // 差分が1[mS]以上の時
                    m_BottomTimer.start(static_cast<int>(nextInterval));
                }
                else {
                    // 差分が0[mS]以下の時
                    m_BottomTimer.start(0);
                }
            }
            else {
                // 書き込み済みのログファイル内にオブジェクトが存在しない場合
                // タイマを再開 (開始時刻を更新)
                m_BottomTimer.start(static_cast<int>(m_Bottominterval));
                return;
            }
        }
    }
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
