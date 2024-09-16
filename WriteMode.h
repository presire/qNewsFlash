#ifndef WRITEMODE_H
#define WRITEMODE_H

#include <QObject>
#include <QMutex>
#include <optional>
#include "Article.h"
#include "Poster.h"


// スレッドへの書き込みに必要な情報
struct WRITE_INFO {
    QString         RequestURL;       // 記事を書き込むためのPOSTデータを送信するURL
    QString         ThreadURL;        // 書き込むスレッドのURL
    QString         ThreadTitle;      // 書き込み済みのスレッドのタイトル
    QString         ThreadXPath;      // スレッドのレス数を取得するためのXPath
    QString         ExpiredXpath;     // スレッドが落ちた時のスレッドタイトル名を取得するXPath
                                      // 掲示板上の新規作成したスレッドのタイトルを取得する場合にも使用
    QString         ExpiredElement;   // スレッドが落ちた時のスレッドタイトル名 (現在は未使用)
    int             MaxThreadNum;     // スレッドの最大レス数
    bool            ChangeTitle;      // スレッドのタイトルを変更するかどうか
                                      // 防弾嫌儲およびニュース速報(Libre)等のスレッドのタイトルが変更できる掲示板でのみ使用可能
    bool            SaveThread;       // スレッドが5日でdat落ちする基準を変更するかどうか
                                      // 防弾嫌儲およびニュース速報(Libre)等の掲示板でのみ使用可能
    bool            BottomThread;     // スレッドをsage状態に変更するかどうか
                                      // ニュース速報(Libre)等のスレッドの掲示板でのみ使用可能
};


// 書き込み済みのログファイルオブジェクト
struct WRITE_LOG {
    QString     Title;      // 書き込み済みログファイル内にある"thread"オブジェクトの"title"キーの値
    QString     Url;        // 書き込み済みログファイル内にある"thread"オブジェクトの"url"キーの値
    QString     Key;        // 書き込み済みログファイル内にある"thread"オブジェクトの"key"キーの値
    QString     Time;       // 書き込み済みログファイル内にある"thread"オブジェクトの"time"キーの値
    QString     New;        // 書き込み済みログファイル内にある"thread"オブジェクトの"time"キーの値
    bool        bottom;     // 書き込み済みログファイル内にある"thread"オブジェクトの"bottom"キーの値
};


class WriteMode : public QObject
{
    Q_OBJECT

private:    // Variables
    static WriteMode    *m_instance;        // 静的インスタンスポインタ
    static QMutex       m_confMutex;        // シングルトンとファイル操作用のミューテックス
    static QMutex       m_logMutex;         // シングルトンとファイル操作用のミューテックス

    QString             m_SysConfFile;      // qNewsFlashの設定ファイルのパス
    QString             m_LogFile;          // スレッドに書き込み済みのニュース記事を保存するJSONファイルのパス
                                            // qNewsFlash.jsonファイルに設定を記述する
                                            // デフォルト : /var/log/qNewsFlash_log.json
    Article             m_Article;          // スレッドに書き込み済みのニュース記事群 (ログファイルに保存されているニュース記事群のこと)
    THREAD_INFO         m_ThreadInfo;       // 記事を書き込むスレッドの情報
    WRITE_INFO          m_WriteInfo;        // スレッドの書き込みに必要な情報
    QList<WRITE_LOG>    m_WriteLogs;        // 書き込み直後のログファイルオブジェクト群

public:     // Variables
    enum WRITEERROR {
        SUCCEED     = 0,
        POSTERROR   = -1,
        LOGERROR    = -2,
        ETC         = -3
    };

private:    // Methods
    WriteMode(QObject *parent = nullptr);                                   // プライベートコンストラクタ
    ~WriteMode();                                                           // プライベートデストラクタ

    static qint64  getEpocTime();                                           // 現在のエポックタイム (UNIX時刻) を秒単位で取得
    static QString getCurrentTime();                                        // 現在の日本時刻を"yyyy年M月d日 H時m分"形式で取得
    QString        replaceSubjectToken(QString subject,                     // 文字列 %tトークンをスレッドのタイトルに置換
                                       QString title);
    int            checkLastThreadNum();                                    // 書き込むスレッドのレス数が上限に達しているかどうかを確認
    int            getLastThreadNum(const WRITE_INFO &wInfo);               // 書き込むスレッドのレス数を取得
    int            CompareThreadTitle(const QUrl &url,                      // !chttコマンドでスレッドのタイトルが正常に変更されているかどうかを確認
                                      QString &title);                      // !chttコマンドは、防弾嫌儲系の掲示板のみ使用可能
    int            updateThreadJson(const QString &title);                  // スレッド情報 (スレッドのタイトル、スレッドのURL、スレッド番号) を設定ファイルに保存
    int            writeLog(Article       &article,                         // 書き込み済みのニュース記事をJSONファイルに保存
                            const QString &threadtitle,
                            const QString &threadurl,
                            const QString &key,
                            bool          bNewThread = false);
    int            updateDateJsonWrapper(const QString &currentDate);       // (ラッパー向け) 最後にニュース記事を取得した日付を設定ファイルに保存 (フォーマット : "yyyy/M/d")
    int            deleteLogNotTodayWrapper();                              // (ラッパー向け) ログ情報を保存するファイルから、昨日以前(昨日も含む)の書き込み済みのニュース記事を削除
    QList<Article> getDatafromWrittenLogWrapper();                          // (ラッパー向け) ログ情報を保存するファイルから、本日の書き込み済みのニュース記事を取得
                                                                            // また、取得した記事群のデータは、メンバ変数m_WrittenArticlesに保存
                                                                            // ただし、このメソッドは、deleteLogNotToday()メソッドの直後に実行する必要がある
    int            writeBottomLog(const WRITE_LOG &writeLog);               // 書き込み済みログファイル内の該当スレッドに対して、"bottom"キーをtrueへ更新
    int            writeBottomInitialization(const THREAD_INFO &tInfo,      // 書き込み済みログファイル内の該当スレッドに対して、"bottom"キーをtrueへ更新
                                             const WRITE_INFO  &wInfo,
                                             const QString     &key);

    void           setWriteLogData();
    bool           compareTimeStrings(const QString &time1,                 // 指定された時間を超えているかどうかを確認
                                      const QString &time2,
                                      int thresholdMilliSec);


#if (QNEWSFLASH_VERSION_MAJOR == 0 && QNEWSFLASH_VERSION_MINOR < 1)
    int            writeJSON(Article &article);                             // 選択したニュース記事をJSONファイルに保存
    int            truncateJSON();                                          // スレッド書き込み用のJSONファイルの内容を空にする
#endif

public:     // Methods
    WriteMode(const WriteMode&)             = delete;                       // コピーコンストラクタの禁止
    WriteMode& operator=(const WriteMode&)  = delete;                       // 代入の禁止

    static WriteMode* getInstance();                                        // シングルトンインスタンスを取得するための静的メソッド
    void            setSysConfFile(const QString &confFile);                // qNewsFlashの設定ファイルを指定
    void            setLogFile(const QString &logFile);                     // 書き込みに成功したニュース記事の情報を保存するログファイルを指定
    void            setArticle(const Article &object);                      // 書き込むニュース記事を指定
    void            setArticle(const QString &title,                        // 書き込むニュース記事を指定
                               const QString &paragraph,
                               const QString &link,
                               const QString &pubDate);
    void            setThreadInfo(const THREAD_INFO &object);               // 書き込むスレッド情報を指定
    void            setWriteInfo(const WRITE_INFO &object);                 // スレッドの書き込みに必要な情報を指定
    THREAD_INFO     getThreadInfo() const;                                  // 書き込むスレッド情報を取得
    WRITE_INFO      getWriteInfo() const;                                   // スレッドの書き込みに必要な情報を取得
    int             writeMode1();                                           // 書き込みモード 1 : 1つのスレッドにニュース記事および時事ドットコムの速報ニュースを書き込むモード
    int             writeMode2();                                           // 書き込みモード 2 : ニュース記事および時事ドットコムの速報ニュースにおいて、常に新規スレッドを立てるモード
    int             updateDateJson(const QString &currentDate);             // 最後にニュース記事を取得した日付を設定ファイルに保存 (フォーマット : "yyyy/M/d")
    int             deleteLogNotToday();                                    // ログ情報を保存するファイルから、昨日以前(昨日も含む)の書き込み済みのニュース記事を削除
    QList<Article>  getDatafromWrittenLog();                                // ログ情報を保存するファイルから、本日の書き込み済みのニュース記事を取得
                                                                            // また、取得した記事群のデータは、メンバ変数m_WrittenArticlesに保存
                                                                            // ただし、このメソッドは、deleteLogNotToday()メソッドの直後に実行する必要がある
    std::optional<WRITE_LOG> getOldestWriteLog() const;                     // 最も早く新規スレッドを立てた書き込み済みログ情報を取得
    QString         getOldestWriteLogDate() const;                          // 最も早く新規スレッドを立てた日時を取得
    int             writeBottom();                                          // 任意の時間が過ぎた書き込み済みスレッドに対して、レスが無い場合は!bottomコマンドを書き込む
    int             writeBottomLogInitialization(THREAD_INFO tInfo,         // 任意の時間が過ぎた書き込み済みスレッドに対して、レスが無い場合は!bottomコマンドを書き込む
                                                 WRITE_INFO  wInfo,
                                                 int         thresholdMilliSec);

signals:

};


#endif // WRITEMODE_H
