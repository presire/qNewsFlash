#ifndef KYODOFLASH_H
#define KYODOFLASH_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <tuple>
#include <memory>


// 時事ドットコムの速報記事を取得するための情報
struct KYODOFLASHINFO
{
    KYODOFLASHINFO() {};

    QString FlashUrl,       // 速報記事の一覧が存在するURL
            BasisURL;       // 速報記事の基準となるURL
    QString FlashXPath,     // 速報記事が記載されているURLから速報記事のリンクを取得するXPath
            TitleXPath,     // 各速報記事のURLから記事のタイトルを取得するXPath
            ParaXPath,      // 各速報記事のURLから記事の本文を取得するXPath
            PubDateXPath;   // 各速報記事のURLから記事の公開日を取得するXPath
};


class KyodoFlash : public QObject
{
    Q_OBJECT

private:    // Variables
    std::unique_ptr<QNetworkAccessManager>  m_pManager;             // 時事ドットコムから速報記事を取得するためのネットワークオブジェクト
    struct KYODOFLASHINFO                   m_FlashInfo;            // 時事ドットコムの速報記事を取得するための情報
    long long                               m_MaxParagraph;         // 本文の一部を抜粋する場合の最大文字数
    QString                                 m_Title,                // 速報記事のタイトル
                                            m_Paragraph,            // 速報記事の本文
                                            m_URL,                  // 速報記事のURL
                                            m_Date;                 // 速報記事の公開日

private:    // Methods
    static QString  convertDate(QString &strDate);                  // 共同通信の日付形式を"yyyy年M月d日 H時m分"に変換

public:     // Methods
    explicit KyodoFlash(QObject *parent = nullptr) = delete;        // コンストラクタ
    explicit KyodoFlash(long long maxPara,                          // コンストラクタ
                        KYODOFLASHINFO Info,
                        QObject *parent = nullptr);
    ~KyodoFlash() override;                                         // デストラクタ
    int             FetchFlash();                                   // 共同通信から速報記事を取得する
    [[nodiscard]] std::tuple<QString, QString, QString, QString>    // フォーマットに合わせた速報記事を取得する
    getArticleData() const;
};

#endif // KYODOFLASH_H
