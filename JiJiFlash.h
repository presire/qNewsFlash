#ifndef JIJIFLASH_H
#define JIJIFLASH_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <tuple>
#include <memory>


// 時事ドットコムの速報記事を取得するための情報
struct JIJIFLASHINFO
{
    JIJIFLASHINFO() {};

    QString BasisURL,       // 速報記事の基準となるURL
            FlashUrl;       // 速報記事の一覧が存在するURL
    QString FlashXPath,     // 速報記事が記載されているURLから速報記事のリンクを取得するXPath
            TitleXPath,     // 各速報記事のURLから記事のタイトルを取得するXPath
            ParaXPath,      // 各速報記事のURLから記事の本文を取得するXPath
            PubDateXPath,   // 各速報記事のURLから記事の公開日を取得するXPath
            UrlXPath;       // 各速報記事のURLから"<この速報の記事を読む>"の部分のリンクを取得するXPath
                            // このリンクが存在する場合は本記事が存在すると看做す
                            // つまり、本記事が存在する場合は速報記事ではないものとする
};


// 時事ドットコムの速報記事を取得するためのクラス
class JiJiFlash : public QObject
{
    Q_OBJECT

private:    // Variables
    std::unique_ptr<QNetworkAccessManager>  m_pManager;             // 時事ドットコムから速報記事を取得するためのネットワークオブジェクト
    struct JIJIFLASHINFO                    m_FlashInfo;            // 時事ドットコムの速報記事を取得するための情報
    long long                               m_MaxParagraph;         // 本文の一部を抜粋する場合の最大文字数
    QString                                 m_Title,                // 速報記事のタイトル
                                            m_Paragraph,            // 速報記事の本文
                                            m_URL,                  // 速報記事のURL
                                            m_Date;                 // 速報記事の公開日

public:     // Variables

private:    // Methods
    static QString  convertDate(QString &strDate);                  // 日付をISO8601形式から"yyyy年M月d日 H時m分"に変換

public:     // Methods
    explicit JiJiFlash(QObject *parent = nullptr) = delete;         // コンストラクタ
    explicit JiJiFlash(long long maxPara,                           // コンストラクタ
                       JIJIFLASHINFO Info,
                       QObject *parent = nullptr);
    ~JiJiFlash() override;                                          // デストラクタ
    int             FetchFlash();                                   // 時事ドットコムから速報記事を取得する
    [[nodiscard]] std::tuple<QString, QString, QString, QString>    // フォーマットに合わせた速報記事を取得する
                    getArticleData() const;

signals:

};

#endif // JIJIFLASH_H
