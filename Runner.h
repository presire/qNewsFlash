#ifndef RUNNER_H
#define RUNNER_H

#include <QThread>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

#ifdef Q_OS_LINUX
    #include <QSocketNotifier>
#elif Q_OS_WIN
    #include <QWinEventNotifier>
    #include <windows.h>
#endif

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <tuple>
#include <memory>
#include "JiJiFlash.h"
#include "Article.h"
#include "WriteMode.h"
#include "Poster.h"


class Runner : public QObject
{
    Q_OBJECT

private:  // Variables
    // 共通
    QStringList                             m_args;             // コマンドラインオプション
    QString                                 m_User;             // このソフトウェアを実行しているユーザ名
    QString                                 m_SysConfFile;      // このソフトウェアの設定ファイルのパス

#ifdef Q_OS_LINUX
    std::unique_ptr<QSocketNotifier>        m_pNotifier;        // このソフトウェアを終了するためのキーボードシーケンスオブジェクト
#elif Q_OS_WIN
    std::unique_ptr<QWinEventNotifier>      m_pNotifier;        // このソフトウェアを終了するためのキーボードシーケンスオブジェクト
#endif

    std::atomic<bool>                       m_stopRequested;    // キーボードの終了シーケンスフラグ

    // ニュース記事の情報
    bool                                    m_AutoFetch;        // ワンショット機能の有効 / 無効
                                                                // メンバ変数m_intervalの値を使用して自動的にニュース記事を取得するかどうか

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
    QString                                 m_WriteFile;        // スレッド書き込み用のJSONファイルのパス
#endif

    // インターバルタイマ
    // 通常実行 または Systemdサービスで起動している場合に使用する
    QTimer                                  m_timer;            // ニュース記事を取得するためのインターバル時間をトリガとするタイマ
    QTimer                                  m_JiJiTimer;        // 時事ドットコムの速報記事を取得するためのインターバル時間をトリガとするタイマ
    QTimer                                  m_BottomTimer;      // スレッドに!bottomコマンドを書き込むためのインターバル時間をトリガとするタイマ
                                                                // 書き込みモード2 および 書き込みモード3の一般ニュース記事のみ
    unsigned long long                      m_interval;         // ニュース記事を取得する時間間隔
    unsigned long long                      m_JiJiinterval;     // 時事ドットコムから速報ニュースを取得する時間間隔
    unsigned long long                      m_Bottominterval;   // スレッドに!bottomコマンドを書き込む時間間隔

    // News API (ニュースサイト)
    bool                                    m_bNewsAPI;         // News APIからニュース記事を取得するかどうか
    QString                                 m_API;              // News APIからニュース記事を取得するためのキー
    QString                                 m_NewsAPIRSS;       // News APIからニュース記事を取得するためのRSS (URL)
    QStringList                             m_ExcludeMedia;     // News APIからニュース記事を取得する場合において、除外するメディアのURL

    // 時事通信および時事通信の速報 (時事ドットコム) (ニュースサイト)
    bool                                    m_bJiJi;            // 時事ドットコムからニュース記事を取得するかどうか
    QString                                 m_JiJiRSS;          // 時事ドットコムからニュース記事を取得するためのRSS (URL)
    bool                                    m_bJiJiFlash;       // 時事ドットコムから速報ニュースを取得するかどうか
    JIJIFLASHINFO                           m_JiJiFlashInfo;    // 時事ドットコムの速報ニュースの取得に必要な情報

    // 共同通信 (ニュースサイト)
    bool                                    m_bKyodo;           // 共同通信からニュース記事を取得するかどうか
    QString                                 m_KyodoRSS;         // 共同通信からニュース記事を取得するためのRSS (URL)
    bool                                    m_KyodoNewsOnly;    // 共同通信から取得するニュース記事の種類をニュースのみに絞るかどうか
                                                                // ニュース以外の記事では、ビジネス関連やライフスタイル等の記事がある

    // 朝日新聞デジタル (ニュースサイト)
    bool                                    m_bAsahi;           // 朝日新聞デジタルからニュース記事を取得するかどうか
    QString                                 m_AsahiRSS;         // 朝日新聞デジタルからニュース記事を取得するためのRSS (URL)

    // CNET Japan (ニュースサイト)
    bool                                    m_bCNet;            // CNET Japanからニュース記事を取得するかどうか
    QString                                 m_CNETRSS;          // CNET Japanからニュース記事を取得するためのRSS (URL)
    QString                                 m_CNETParaXPath;    // CNET Japanからニュース記事の概要を取得するためのXPath式
                                                                // RSSにあるニュース記事の概要欄には不要な情報が多いため、
                                                                // 該当するニュース記事のURLから記事の概要のみを抽出している

    // ハンギョレ新聞 (ニュースサイト)
    bool                                    m_bHanJ;            // ハンギョレジャパンからニュース記事を取得するかどうか
    QString                                 m_HanJRSS;          // ハンギョレジャパンからニュース記事の概要を取得するためのRSS (URL)
    QString                                 m_HanJTopURL;       // ハンギョレジャパンのトップページのURL
                                                                // ハンギョレジャパンでは、現在、トップページを基準にニュース記事のURLが存在する

    // ロイター通信 (ニュースサイト)
    bool                                    m_bReuters;         // ロイター通信からニュース記事を取得するかどうか
    QString                                 m_ReutersRSS;       // ロイター通信からニュース記事の概要を取得するためのRSS (URL)
    QString                                 m_ReutersParaXPath; // ロイター通信からニュース記事の概要を取得するためのXPath式
                                                                // RSSにあるニュース記事の概要欄の値が空欄のため、
                                                                // 該当するニュース記事のURLから記事の概要のみを抽出している

    // 東京新聞 (ニュースサイト)
    bool                                    m_bTokyoNP;         // 東京新聞からニュース記事を取得するかどうか
    QString                                 m_TokyoNPTopURL,    // 東京新聞のトップページのURL
                                                                // 東京新聞では、現在、トップページを基準にニュース記事のURLが存在する
                                            m_TokyoNPFetchURL,  // 東京新聞からニュース記事を取得するページのURL
                                                                // 例1. 総合ニュースを取得する場合 : https://www.tokyo-np.co.jp
                                                                // 例2. 政治ニュースのみを取得する場合 : https://www.tokyo-np.co.jp/n/politics
                                                                // 例3. 国際ニュースのみを取得する場合 : https://www.tokyo-np.co.jp/n/world
                                            m_TokyoNPThumb,     // 東京新聞のヘッドライン取得用XPath
                                            m_TokyoNPNews,      // 東京新聞のその他ニュース記事の取得用XPath
                                            m_TokyoNPJSON;      // 各ニュース記事の情報を取得するためのXPath

    // 各ニュースサイトからニュース記事を取得するためのネットワークオブジェクト
    std::unique_ptr<QNetworkAccessManager>  manager;            // ニュース記事を取得するためのネットワークオブジェクト
    QNetworkReply                           *m_pReply;          // News API用HTTPレスポンスのオブジェクト
    QNetworkReply                           *m_pReplyJiJi;      // 時事ドットコム用HTTPレスポンスのオブジェクト
    QNetworkReply                           *m_pReplyKyodo;     // 共同通信用HTTPレスポンスのオブジェクト
    QNetworkReply                           *m_pReplyAsahi;     // 朝日新聞デジタル用HTTPレスポンスのオブジェクト
    QNetworkReply                           *m_pReplyCNet;      // CNET用HTTPレスポンスのオブジェクト
    QNetworkReply                           *m_pReplyHanJ;      // ハンギョレジャパン用HTTPレスポンスのオブジェクト
    QNetworkReply                           *m_pReplyReuters;   // ロイター通信用HTTPレスポンスのオブジェクト

    // ニュース記事群に関する情報
    QList<Article>                          m_BeforeWritingArticles;  // 各ニュースサイトから一時的に取得したニュース記事群 (書き込む前のニュース記事群のこと)
    QList<Article>                          m_WrittenArticles;        // スレッドに書き込み済みのニュース記事群 (ログファイルに保存されているニュース記事群のこと)

    // スレッドに関する情報
    THREAD_INFO                             m_ThreadInfo;       // ニュース記事を書き込むスレッドの情報

    // 書き込み用オブジェクト
    WriteMode                               *m_pWriteMode;      // 書き込み用オブジェクト

    // 書き込みモード
    int                                     m_WriteMode;        // 書き込みモード 1 : 1つのスレッドにニュース記事および時事ドットコムの速報ニュースを書き込むモード
                                                                // 書き込みモード 2 : ニュース記事および時事ドットコムの速報ニュースにおいて、常に新規スレッドを立てるモード
                                                                // 書き込みモード 3 : 時事ドットコムの速報ニュース以外は、常に新規スレッドを立てるモード

    // スレッドの書き込みに関する情報
    WRITE_INFO                              m_WriteInfo;        // スレッドの書き込みに関する情報

    // その他設定ファイルに関する情報
    long long                               m_MaxParagraph;     // 本文の一部を抜粋する場合の最大文字数
    int                                     m_WithinHours;      // 公開日がN時間前以内のニュース記事を取得する
    QString                                 m_LastUpdate;       // 最後にニュース記事群を取得した日付 (フォーマット : yyyy/M/d)
    QString                                 m_LogFile;          // スレッドに書き込み済みのニュース記事を保存するJSONファイルのパス
                                                                // qNewsFlash.jsonファイルに設定を記述する
                                                                // デフォルト : /var/log/qNewsFlash_log.json

public:  // Variables

private:  // Methods
    int            getConfiguration(QString &filepath);         // このソフトウェアの設定ファイルの情報を取得

#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
    int            checkLogFile();                              // このソフトウェアのログ情報を保存するファイルのパスを設定
                                                                // ログ情報とは、本日の書き込み済みのニュース記事群を指す
#endif

    static int     checkLogFile(QString &filepath);             // このソフトウェアのログ情報を保存するファイルのパスを設定
                                                                // ログ情報とは、書き込み済みのニュース記事を指す
    void           itemTagsforJiJi(xmlNode *a_node);            // 時事ドットコムのニュース記事(RSS)を分解して取得
    void           itemTagsforKyodo(xmlNode *a_node);           // 共同通信のニュース記事(RSS)を分解して取得
    void           itemTagsforAsahi(xmlNode *a_node);           // 朝日新聞デジタルのニュース記事(RSS)を分解して取得
    void           itemTagsforCNet(xmlNode *a_node);            // CNET Japanのニュース記事(RSS)を分解して取得
    void           itemTagsforHanJ(xmlNode *a_node);            // ハンギョレジャパンのニュース記事(RSS)を分解して取得
    void           itemTagsforReuters(xmlNode *a_node);         // ロイター通信のニュース記事(RSS)を分解して取得
    static QString convertJPDate(QString &strDate);             // News APIのニュース記事にある日付を日本時間および"yyyy/M/d h時m分"に変換
    static QString convertJPDateforKyodo(QString &strDate);     // 共同通信のニュース記事にある日付を日本時間および"yyyy/M/d h時m分"に変換
    static QString convertDate(QString &strDate);               // 時事ドットコム、ロイター通信のニュース記事にある日付を"yyyy年M月d日 H時m分"に変換
    static QString convertDateHanJ(QString &strDate);           // ハンギョレジャパンのニュース記事にある日付を"yyyy年M月d日 H時m分"に変換
    static bool    isToday(const QString &dateString);          // ニュース記事が今日の日付かどうかを確認
    bool           isHoursAgo(const QString &dateString) const; // ニュース記事が数時間前の日付かどうかを確認
    Article        selectArticle();                             // 取得したニュース記事群からランダムで1つを選択

public:  // Methods

#ifdef Q_OS_LINUX
    explicit    Runner(QStringList args, QString user, QObject *parent = nullptr);
#elif Q_OS_WIN
    explicit    Runner(QStringList args, QObject *parent = nullptr);
#endif

    ~Runner() override = default;

signals:
    void NewAPIfinished();      // News APIからニュース記事の取得の終了を知らせるためのシグナル
    void JiJifinished();        // 時事ドットコムからニュース記事の取得の終了を知らせるためのシグナル
    void Kyodofinished();       // 共同通信からニュース記事の取得の終了を知らせるためのシグナル
    void Asahifinished();       // 朝日新聞デジタルからニュース記事の取得の終了を知らせるためのシグナル
    void CNetfinished();        // CNET Japanからニュース記事の取得の終了を知らせるためのシグナル
    void HanJfinished();        // ハンギョレジャパンからニュース記事の取得の終了を知らせるためのシグナル
    void Reutersfinished();     // ロイター通信からニュース記事の取得の終了を知らせるためのシグナル
    void TokyoNPfinished();     // 東京新聞からニュース記事の取得の終了を知らせるためのシグナル

public slots:
    void run();                     // このソフトウェアを最初に実行する時にのみ実行するメイン処理
    void fetchNonBreakingNews();    // 速報ニュース以外のニュース記事の取得するスロット
    void fetchNewsAPI();            // News APIからニュース記事の取得後に実行するスロット
    void fetchJiJiRSS();            // 時事ドットコムからニュース記事の取得後に実行するスロット
    void fetchKyodoRSS();           // 共同通信からニュース記事の取得後に実行するスロット
    void fetchAsahiRSS();           // 朝日新聞デジタルからニュース記事の取得後に実行するスロット
    void fetchCNetRSS();            // CNET Japanからニュース記事の取得後に実行するスロット
    void fetchHanJRSS();            // ハンギョレジャパンからニュース記事の取得後に実行するスロット
    void fetchReutersRSS();         // ロイター通信からニュース記事の取得後に実行するスロット
    void fetchTokyoNP();            // 東京新聞からニュース記事の取得後に実行するスロット
    void JiJiFlashfetch();          // 時事ドットコムから速報記事の取得するスロット
    void bottomThread();            // 書き込み済みのスレッドに!bottomコマンドを書き込むスロット
    void onReadyRead();             // ノンブロッキングでキー入力を受信するスロット
};

#endif // RUNNER_H
