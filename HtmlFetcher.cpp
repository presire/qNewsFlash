#include <QTextCodec>
#include <iostream>
#include <regex>
#include "HtmlFetcher.h"


HtmlFetcher::HtmlFetcher(QObject *parent) : m_pManager(std::make_unique<QNetworkAccessManager>(this)), QObject{parent}
{

}


HtmlFetcher::HtmlFetcher(long long maxParagraph, QObject *parent) : m_MaxParagraph(maxParagraph),
    m_pManager(std::make_unique<QNetworkAccessManager>(this)), QObject{parent}
{

}


HtmlFetcher::~HtmlFetcher() = default;


// 指定されたURLが存在するかどうかを確認する
bool HtmlFetcher::checkUrlExistence(const QUrl &url)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager();

    // HEADリクエストの作成
    QNetworkRequest request(url);

    // リダイレクトの無効
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, false);

    // HEADを取得
    QNetworkReply *pReply = manager->get(request);

    // レスポンス待機
    QEventLoop loop;
    QObject::connect(pReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    // レスポンスの確認
    if (pReply->error() == QNetworkReply::NoError) {
        // ステータスコードの確認
        int statusCode = pReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 200) {
            pReply->deleteLater();
            return true;
        }
    }

    pReply->deleteLater();

    return false;
}


// ニュース記事のURLにアクセスして、本文を取得する
int HtmlFetcher::fetch(const QUrl &url, bool redirect, const QString& _xpath)
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


// ニュース記事の本文を取得する
int HtmlFetcher::fetchParagraph(QNetworkReply *reply, const QString& _xpath)
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
    // libxml2ではエンコーディングの自動判定において問題があるため、エンコーディングを明示的に指定する
    xmlDocPtr doc = htmlReadDoc((const xmlChar*)htmlContent.toStdString().c_str(), nullptr, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (doc == nullptr) {
        std::cerr << "Document not parsed successfully" << std::endl;
        reply->deleteLater();

        return -1;
    }

    // XPathで特定の要素を検索
    auto *xpath = xmlStrdup((const xmlChar*)_xpath.toUtf8().constData());
    xmlXPathObjectPtr result = getNodeset(doc, xpath);
    if (result == nullptr) {
        std::cerr << "Error in getNodeset" << std::endl;
        xmlFreeDoc(doc);
        reply->deleteLater();

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
    m_Paragraph = content.size() > m_MaxParagraph ? m_Paragraph = content.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : content;

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


// ニュース記事のURLにアクセスして、XPathで指定した値を取得する
int HtmlFetcher::fetchElement(const QUrl &url, bool redirect, const QString &_xpath, int elementType)
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

    // レスポンスの取得
    if (pReply->error() != QNetworkReply::NoError) {
        std::cerr << QString("エラー : %1").arg(pReply->errorString()).toStdString() << std::endl;
        pReply->deleteLater();

        return -1;
    }

    QString htmlContent = pReply->readAll();

    // libxml2の初期化
    xmlInitParser();
    LIBXML_TEST_VERSION

    // 文字列からHTMLドキュメントをパース
    // libxml2ではエンコーディングの自動判定において問題があるため、エンコーディングを明示的に指定する
    xmlDocPtr doc = htmlReadDoc((const xmlChar*)htmlContent.toStdString().c_str(), nullptr, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (doc == nullptr) {
        std::cerr << QString("エラー : HTMLドキュメントのパースに失敗").toStdString() << std::endl;
        pReply->deleteLater();

        return -1;
    }

    // XPathで特定の要素を検索
    auto *xpath = xmlStrdup((const xmlChar*)_xpath.toUtf8().constData());
    xmlXPathObjectPtr result = getNodeset(doc, xpath);
    if (result == nullptr) {
        std::cerr << QString("エラー : ノードの取得に失敗").toStdString() << std::endl;
        xmlFreeDoc(doc);
        pReply->deleteLater();

        return -1;
    }

    // 結果のノードセットからテキストを取得
    m_Element.clear();
    xmlNodeSetPtr nodeset = result->nodesetval;
    for (auto i = 0; i < nodeset->nodeNr; ++i) {
        xmlNodePtr cur = nodeset->nodeTab[i]->xmlChildrenNode;
        while (cur != nullptr) {
            if (cur->type == elementType) {
                auto buffer = QString(((const char*)cur->content)) + QString(" ");
                m_Element.append(buffer);
            }
            cur = cur->next;
        }
    }

    // libxml2オブジェクトの破棄
    xmlXPathFreeObject(result);
    xmlFreeDoc(doc);

    // libxml2のクリーンアップ
    xmlCleanupParser();

    pReply->deleteLater();

    return 0;
}


// 書き込むスレッドの最後尾のレス番号を取得する
int HtmlFetcher::fetchLastThreadNum(const QUrl &url, bool redirect, const QString &_xpath, int elementType)
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

    // レスポンスの取得
    if (pReply->error() != QNetworkReply::NoError) {
        std::cerr << QString("エラー : %1").arg(pReply->errorString()).toStdString() << std::endl;
        pReply->deleteLater();

        return -1;
    }

    QString htmlContent = pReply->readAll();

    // libxml2の初期化
    xmlInitParser();
    LIBXML_TEST_VERSION

    // 文字列からHTMLドキュメントをパース
    // libxml2ではエンコーディングの自動判定において問題があるため、エンコーディングを明示的に指定する
    xmlDocPtr doc = htmlReadDoc((const xmlChar*)htmlContent.toStdString().c_str(), nullptr, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (doc == nullptr) {
        std::cerr << QString("エラー : HTMLドキュメントのパースに失敗").toStdString() << std::endl;
        pReply->deleteLater();

        return -1;
    }

    // XPathで特定の要素を検索
    auto *xpath = xmlStrdup((const xmlChar*)_xpath.toUtf8().constData());
    xmlXPathObjectPtr result = getNodeset(doc, xpath);
    if (result == nullptr) {
        std::cerr << QString("エラー : ノードの取得に失敗").toStdString() << std::endl;
        xmlFreeDoc(doc);
        pReply->deleteLater();

        return -1;
    }

    // 結果のノードセットからテキストを取得
    m_Element.clear();
    xmlNodeSetPtr nodeset = result->nodesetval;
//    for (auto i = 0; i < nodeset->nodeNr; ++i) {
//        xmlNodePtr cur = nodeset->nodeTab[i]->xmlChildrenNode;
//        while (cur != nullptr) {
//            if (cur->type == elementType) {
//                auto buffer = QString(((const char*)cur->content)) + QString(" ");
//                m_Element.append(buffer);
//            }
//            cur = cur->next;
//        }
//    }
    xmlNodePtr cur = nodeset->nodeTab[nodeset->nodeNr - 1]->xmlChildrenNode;
    if (cur->type == elementType) {
        auto buffer = QString(((const char*)cur->content));
        m_Element.append(buffer);
    }

    // libxml2オブジェクトの破棄
    xmlXPathFreeObject(result);
    xmlFreeDoc(doc);

    // libxml2のクリーンアップ
    xmlCleanupParser();

    pReply->deleteLater();

    return 0;
}


// 新規作成したスレッドからスレッドのパスおよびスレッド番号を取得する
int HtmlFetcher::extractThreadPath(const QString &htmlContent, const QString &bbs)
{
    // HTMLコンテンツをパース
    htmlDocPtr doc = htmlReadDoc((const xmlChar*)htmlContent.toStdString().c_str(), nullptr, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);

    // XPathコンテキストを作成
    xmlXPathContextPtr context = xmlXPathNewContext(doc);

    // http-equivが"Refresh"であるmetaタグを見つけるXPathクエリ
    xmlXPathObjectPtr result = xmlXPathEvalExpression((xmlChar*)"//meta[@http-equiv='Refresh']", context);

    if(result != nullptr && result->nodesetval != nullptr) {
        for(int i = 0; i < result->nodesetval->nodeNr; i++) {
            xmlNodePtr node = result->nodesetval->nodeTab[i];

            // content属性を取得
            xmlChar* content = xmlGetProp(node, (xmlChar*)"content");
            std::string contentStr((char*)content);
            xmlFree(content);

            // URLからスレッドパスを抽出
            /// まず、URLの部分を抽出
            /// <数値>;URL=/<ディレクトリ名  例. /path/to/test/read.cgi>/<BBS名>/<スレッド番号  例.  15891277>/<その他スレッドの情報  例. l10#bottom>
            QString url(contentStr.c_str());
            static QRegularExpression re1("URL=(.*)");
            QRegularExpressionMatch urlMatch = re1.match(url);
            if (urlMatch.hasMatch()) {
                // 1番目のキャプチャグループ (URL以降の文字列) を取得
                url = urlMatch.captured(1);
            }

            /// 次に、抽出したURLの部分からスレッドのパスを抽出
            /// 正規表現パターン : 最後の "/" 以前を取得する
            static QRegularExpression regExThreadPath("^(.+/)[^/]+");
            QRegularExpressionMatch MatchThreadPath = regExThreadPath.match(url);
            if (MatchThreadPath.hasMatch()) {
                m_ThreadPath = MatchThreadPath.captured(1);

#ifdef _DEBUG
                std::cout << QString("スレッドのパス : %1").arg(m_ThreadPath).toStdString() << std::endl;
#endif
            }

            /// さらに、抽出したURLの部分からスレッド番号を抽出
            static QRegularExpression regExThreadNum(QString("/%1/([^/]+)/").arg(bbs));
            QRegularExpressionMatch MatchThreadNum = regExThreadNum.match(url);
            if (MatchThreadNum.hasMatch()) {
                m_ThreadNum = MatchThreadNum.captured(1);

#ifdef _DEBUG
                std::cout << "スレッド番号 : " << m_ThreadNum.toStdString() << std::endl;
#endif
            }

            /// さらに、抽出したURLの部分からスレッド番号を抽出 (C++標準ライブラリを使用する場合)
            /// デッドコードではあるが、処理をコメントアウトして一時的に保存する
//            auto urlStr = url.toStdString();
//            std::regex regExThreadNum(QString("/[^/]+/%1/([^/]+)/").arg(bbs).toStdString());
//            std::smatch ThreadNumMatch;
//            if (std::regex_search(urlStr, ThreadNumMatch, regExThreadNum) && ThreadNumMatch.size() > 1) {
//                // スレッド番号を取得
//                m_ThreadNum = ThreadNumMatch[1].str().c_str();

//#ifdef _DEBUG
//                std::cout << "スレッド番号 : " << m_ThreadNum.toStdString() << std::endl;
//#endif
//            }
        }
    }

    xmlXPathFreeObject(result);
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);

    // スレッドのパスおよびスレッド番号の取得に失敗した場合はエラーとする
    if (m_ThreadPath.isEmpty() && m_ThreadNum.isEmpty()) return -1;

    return 0;
}


// 取得したニュース記事の本文の一部を渡す
QString HtmlFetcher::getParagraph() const
{
    return m_Paragraph;
}


// スレッドのパスを取得する
QString HtmlFetcher::GetThreadPath() const
{
    return m_ThreadPath;
}


// スレッド番号を取得する
QString HtmlFetcher::GetThreadNum() const
{
    return m_ThreadNum;
}


// エレメントを取得する
QString HtmlFetcher::GetElement() const
{
    return m_Element;
}
