#ifndef HTMLFETCHER_H
#define HTMLFETCHER_H

#include <QObject>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <memory>


class HtmlFetcher : public QObject
{
    Q_OBJECT

private:  // Variables
    long long                               m_MaxParagraph{};
    std::unique_ptr<QNetworkAccessManager>  m_pManager;
    QString                                 m_Paragraph;
    QString                                 m_ThreadPath,                               // スレッドのパス
                                            m_ThreadNum;                                // スレッド番号
    QString                                 m_Element;                                  // XPathを使用して取得するエレメント

private:  // Methods
    int fetchParagraph(QNetworkReply *reply, const QString& _xpath);                    // ニュース記事の本文を取得する
    xmlXPathObjectPtr getNodeset(xmlDocPtr doc, const xmlChar *xpath);                  // ニュース記事の本文を取得する

public:   // Methods
    explicit HtmlFetcher(QObject *parent = nullptr);
    explicit HtmlFetcher(long long maxParagraph, QObject *parent = nullptr);
     ~HtmlFetcher() override;
    bool    checkUrlExistence(const QUrl &url);                                         // 指定されたURLが存在するかどうかを確認する
    int     fetch(const QUrl &url, bool redirect = false,                               // ニュース記事のURLにアクセスして、本文を取得する
                  const QString& _xpath = "//head/meta[@name='description']/@content");
    int     fetchElement(const QUrl &url, bool redirect, const QString &_xpath,         // ニュース記事のURLにアクセスして、XPathで指定した値を取得する
                         int elementType);
    int     fetchLastThreadNum(const QUrl &url, bool redirect, const QString &_xpath,   // 書き込むスレッドの最後尾のレス番号を取得する
                               int elementType);
    int     extractThreadPath(const QString &htmlContent, const QString &bbs);          // 新規作成したスレッドからスレッドのパスおよびスレッド番号を抽出する
    [[nodiscard]] QString  getParagraph() const;                                        // 取得したニュース記事の本文の一部を渡す
    [[nodiscard]] QString GetThreadPath() const;                                        // スレッドのパスを取得する
    [[nodiscard]] QString GetThreadNum() const;                                         // スレッド番号を取得する
    [[nodiscard]] QString GetElement() const;                                           // エレメントを取得する

signals:

public slots:

};

#endif // HTMLFETCHER_H
