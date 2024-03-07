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
    unsigned long long                      m_MaxParagraph;
    std::unique_ptr<QNetworkAccessManager>  m_pManager;
    QString                                 m_Paragraph;

private:  // Methods
    int fetchParagraph(QNetworkReply *reply, QString _xpath);
    xmlXPathObjectPtr getNodeset(xmlDocPtr doc, const xmlChar *xpath);

public:   // Methods
    explicit HtmlFetcher(unsigned long long maxParagraph, QObject *parent = nullptr);
    virtual  ~HtmlFetcher();
    int      fetch(const QUrl &url, bool redirect = false, QString _xpath = "//head/meta[@name='description']/@content");
    QString  getParagraph() const;

signals:

public slots:

};

#endif // HTMLFETCHER_H
