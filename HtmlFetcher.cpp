#include <iostream>
#include "HtmlFetcher.h"


HtmlFetcher::HtmlFetcher(unsigned long long maxParagraph, QObject *parent) : m_MaxParagraph(maxParagraph),
    m_pManager(std::make_unique<QNetworkAccessManager>(this)) , QObject{parent}
{

}


HtmlFetcher::~HtmlFetcher()
{

}


int HtmlFetcher::fetch(const QUrl &url, bool redirect, QString _xpath)
{
    // リダイレクトを自動的にフォロー
    QNetworkRequest request(url);

    if (redirect) {
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, true);
    }

    auto pReply = m_pManager->get(request);

    // レスポンス待機
    QEventLoop loop;
    QObject::connect(pReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    // 本文の一部を取得
    return fetchParagraph(pReply, _xpath);
}


int HtmlFetcher::fetchParagraph(QNetworkReply *reply, QString _xpath)
{
    if (reply->error() != QNetworkReply::NoError) {
        std::cerr << "エラー : " << reply->errorString().toStdString();
        reply->deleteLater();

        return -1;
    }

    QString htmlContent = reply->readAll();

    // libxml2の初期化
    xmlInitParser();
    LIBXML_TEST_VERSION

    // 文字列からHTMLドキュメントをパース
    xmlDocPtr doc = htmlReadDoc((const xmlChar*)htmlContent.toStdString().c_str(), NULL, NULL, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (doc == nullptr) {
        std::cerr << "Document not parsed successfully" << std::endl;
        return -1;
    }

    // XPathで特定の要素を検索
    //xmlChar *xpath = (xmlChar*) "//head/meta[@name='description']/@content";
    auto pXPath = _xpath.toLocal8Bit().data();
    xmlChar *xpath = reinterpret_cast<xmlChar*>(pXPath);
    xmlXPathObjectPtr result = getNodeset(doc, xpath);
    if (result == nullptr) {
        std::cerr << "Error in getNodeset" << std::endl;
        xmlFreeDoc(doc);

        return -1;
    }

    // 結果のノードセットからテキストを取得
    xmlNodeSetPtr nodeset = result->nodesetval;
    QString content = "";

    for (auto i = 0; i < nodeset->nodeNr; ++i) {
        xmlNodePtr cur = nodeset->nodeTab[i]->xmlChildrenNode;
        while (cur != nullptr) {
            if (cur->type == XML_TEXT_NODE) {
                auto buffer = QString(((const char*)cur->content));

                // 不要な文字を削除 (\n, \t, 半角全角スペース等)
                static QRegularExpression regex("[\\s]", QRegularExpression::CaseInsensitiveOption);
                buffer = buffer.replace(regex, "");
                buffer.replace(" ", "").replace("\u3000", "");

                content.append(buffer);
            }
            cur = cur->next;
        }
    }

    // 本文が指定文字数以上の場合、指定文字数分のみを抽出
    m_Paragraph = content.size() > m_MaxParagraph ? m_Paragraph = content.mid(0, m_MaxParagraph - 1) : content;

    xmlXPathFreeObject(result);
    xmlFreeDoc(doc);
    xmlCleanupParser();

    reply->deleteLater();

    return 0;
}


xmlXPathObjectPtr HtmlFetcher::getNodeset(xmlDocPtr doc, const xmlChar *xpath)
{
    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    if (context == nullptr) {
        std::cerr << "Error in xmlXPathNewContext" << std::endl;
        return nullptr;
    }

    xmlXPathObjectPtr result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == nullptr) {
        std::cerr << "Error in xmlXPathEvalExpression" << std::endl;
        return nullptr;
    }

    if (xmlXPathNodeSetIsEmpty(result->nodesetval)) {
        xmlXPathFreeObject(result);
        std::cerr << "No result" << std::endl;
        return nullptr;
    }

    return result;
}


QString HtmlFetcher::getParagraph() const
{
    return m_Paragraph;
}
