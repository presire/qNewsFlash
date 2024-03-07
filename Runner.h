#ifndef RUNNER_H
#define RUNNER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <tuple>
#include <memory>
#include "Article.h"
#include "KeyListener.h"
#include "Poster.h"


class Runner : public QObject
{
    Q_OBJECT

private:  // Variables
    QStringList                             m_args;         // コマンドラインオプション
    QString                                 m_User;         // このソフトウェアを実行しているユーザ名
    QString                                 m_SysConfFile;  // このソフトウェアの設定ファイルのパス
    QString                                 m_WriteFile;    // スレッド書き込み用のJSONファイルのパス
    QString                                 m_LogFile;      // スレッド書き込み済みのJSONファイルのパス
                                                            // qNewsFlash.jsonファイルに設定を記述する
                                                            // デフォルト : /var/log/qNewsFlash_log.json
    QTimer                                  m_timer;        // インターバル時間をトリガとするタイマ
    unsigned long long                      m_interval;     // ニュース記事を取得する時間間隔
    bool                                    m_bNewsAPI;     // News APIからニュース記事を取得するかどうか
    bool                                    m_bJiJi;        // 時事ドットコムからニュース記事を取得するかどうか
    bool                                    m_bKyodo;       // 共同通信からニュース記事を取得するかどうか
    bool                                    m_bAsahi;       // 朝日デジタルからニュース記事を取得するかどうか
    bool                                    m_bCNet;        // CNET Japanからニュース記事を取得するかどうか
    bool                                    m_bHanJ;        // ハンギョレジャパンからニュース記事を取得するかどうか
    QString                                 m_API;          // News APIのキー
    QStringList                             m_ExcludeMedia; // News APIからのニュース記事において、除外するメディアのURL
    unsigned long long                      m_MaxParagraph; // 本文の一部を抜粋する場合の最大文字数
    QString                                 m_LastUpdate;   // 最後にニュース記事群を取得した日付 (フォーマット : yyyy/M/d)
    std::unique_ptr<QNetworkAccessManager>  manager;        // ニュース記事を取得するためのネットワークオブジェクト
    QNetworkReply                           *m_pReply;      // News API用HTTPレスポンスのオブジェクト
    QNetworkReply                           *m_pReplyJiJi;  // 時事ドットコム用HTTPレスポンスのオブジェクト
    QNetworkReply                           *m_pReplyKyodo; // 共同通信用HTTPレスポンスのオブジェクト
    QNetworkReply                           *m_pReplyAsahi; // 朝日デジタル用HTTPレスポンスのオブジェクト
    QNetworkReply                           *m_pReplyCNet;  // CNET用HTTPレスポンスのオブジェクト
    QNetworkReply                           *m_pReplyHanJ;  // ハンギョレジャパン用HTTPレスポンスのオブジェクト
    std::unique_ptr<KeyListener>            m_pListener;    // このソフトウェアを終了するためのキーボードシーケンスオブジェクト
    QList<Article>                          m_BeforeWritingArticles;  // Webから取得したニュース記事群
    QList<Article>                          m_WrittenArticles;        // スレッド書き込み済みの記事群のログ
    QString                                 m_RequestURL;   // 記事を書き込むためのPOSTデータを送信するURL
    THREAD_INFO                             m_ThreadInfo;   // 記事を書き込むスレッドの情報

public:  // Variables

private:  // Methods
    int         getConfiguration(QString &filepath);        // このソフトウェアの設定ファイルの情報を取得
    [[maybe_unused]] void        GetOption();                                // コマンドラインオプションの取得 (現在は未使用)
    [[maybe_unused]] int         setLogFile();                               // このソフトウェアのログ情報を保存するファイルのパスを設定
                                                            // ログ情報とは、本日の書き込み済みのニュース記事群を指す
    static int  setLogFile(QString &filepath);              // このソフトウェアのログ情報を保存するファイルのパスを設定
                                                            // ログ情報とは、書き込み済みのニュース記事を指す
    int         deleteLogNotToday();                        // ログ情報を保存するファイルから、昨日以前(昨日も含む)の書き込み済みのニュース記事を削除
    int         getDatafromWrittenLog();                    // ログ情報を保存するファイルから、本日の書き込み済みのニュース記事を取得
                                                            // また、取得した記事群のデータは、メンバ変数m_WrittenArticlesに保存
                                                            // ただし、このメソッドは、deleteLogNotToday()メソッドの直後に実行する必要がある
    void        itemTagsforJiJi(xmlNode *a_node);           // 時事ドットコムのニュース記事(RSS)を分解して取得
    void        itemTagsforKyodo(xmlNode *a_node);          // 共同通信のニュース記事(RSS)を分解して取得
    void        itemTagsforAsahi(xmlNode *a_node);          // 朝日デジタルのニュース記事(RSS)を分解して取得
    void        itemTagsforCNet(xmlNode *a_node);           // CNET Japanのニュース記事(RSS)を分解して取得
    void        itemTagsforHanJ(xmlNode *a_node);           // ハンギョレジャパンのニュース記事(RSS)を分解して取得
    QString     convertJPDate(QString &strDate);            // News APIのニュース記事にある日付を日本時間および"yyyy/M/d h時m分"に変換
    QString     convertJPDateforKyodo(QString &strDate);    // News APIのニュース記事にある日付を日本時間および"yyyy/M/d h時m分"に変換
    QString     convertDate(QString &strDate);              // 時事ドットコムのニュース記事にある日付を"yyyy年M月d日 H時m分"に変換
    QString     convertDateHanJ(QString &strDate);          // ハンギョレジャパンのニュース記事にある日付を"yyyy年M月d日 H時m分"に変換
    static bool isToday(const QString &dateString);         // ニュース記事が今日の日付かどうかを確認
    Article     selectArticle();                            // 取得したニュース記事群からランダムで1つを選択
#ifdef _BELOW_0_1_0
    int         writeJSON(Article &article);                // 選択したニュース記事をJSONファイルに保存
    int         truncateJSON();                             // スレッド書き込み用のJSONファイルの内容を空にする
#endif
    int         writeLog(Article &article);                 // 書き込み済みのニュース記事をJSONファイルに保存
    int         updateDateJson(const QString &currentDate); // 最後にニュース記事を取得した日付 (フォーマット : "yyyy/M/d")

public:  // Methods
    explicit    Runner(QStringList args, QObject *parent = nullptr);
    explicit    Runner(QStringList args, QString user, QObject *parent = nullptr);
    ~Runner() override = default;

signals:
    void NewAPIfished();    // News APIからニュース記事の取得の終了を知らせるためのシグナル
    void JiJifinished();    // 時事ドットコムからニュース記事の取得の終了を知らせるためのシグナル
    void Kyodofinished();   // 共同通信からニュース記事の取得の終了を知らせるためのシグナル
    void Asahifinished();   // 朝日デジタルからニュース記事の取得の終了を知らせるためのシグナル
    void CNetfinished();    // CNET Japanからニュース記事の取得の終了を知らせるためのシグナル
    void HanJfinished();    // ハンギョレジャパンからニュース記事の取得の終了を知らせるためのシグナル

public slots:
    void run();             // このソフトウェアを最初に実行する時にのみ実行するメイン処理
    void fetch();           // ニュース記事の取得
    void fetchNewsAPI();    // News APIからニュース記事の取得後に実行するスロット
    void fetchJiJiRSS();    // 時事ドットコムからニュース記事の取得後に実行するスロット
    void fetchKyodoRSS();   // 共同通信からニュース記事の取得後に実行するスロット
    void fetchAsahiRSS();   // 朝日デジタルからニュース記事の取得後に実行するスロット
    void fetchCNetRSS();    // CNET Japanからニュース記事の取得後に実行するスロット
    void fetchHanJRSS();    // ハンギョレジャパンからニュース記事の取得後に実行するスロット
};

#endif // RUNNER_H
