#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    #include <QStringEncoder>
#else
    #include <QTextCodec>
#endif

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <iconv.h>
#include <iostream>
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
    QNetworkAccessManager manager;

    // HEADリクエストの作成
    QNetworkRequest request(url);

    // リダイレクトの無効
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, false);

    // HEADを取得
    QNetworkReply *pReply = manager.get(request);

    // レスポンス待機
    QEventLoop loop;
    QObject::connect(pReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    // レスポンスの確認
    // 例: Webサーバのコンテンツが見つからない場合は、QNetworkReply::ContentNotFoundErrorが返る (HTTPエラー404と同様)
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


// 指定されたURLが存在するかどうかを確認する
/// スレッドが生存している場合 : 0
/// スレッドが落ちている場合 : 1
/// スレッド情報の取得に失敗した場合 : -1
int HtmlFetcher::checkUrlExistence(const QUrl &url, const QString ExpiredElement, const QString ExpiredXPath, bool shiftjis)
{
    // スレッドのURLが無い場合
    if (url.isEmpty()) {
        return 1;
    }

    // まず、Webページが存在するかどうかを確認する
    QNetworkAccessManager manager;

    /// HEADリクエストの作成
    QNetworkRequest request(url);

    /// リダイレクトの無効
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, false);

    /// HEADを取得
    QNetworkReply *pReply = manager.get(request);

    /// レスポンス待機
    QEventLoop loop;
    QObject::connect(pReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    /// レスポンスの確認
    /// 例: Webページが存在しない場合は、QNetworkReply::ContentNotFoundErrorが返る (HTTPエラー404と同様)
    if (pReply->error() == QNetworkReply::ContentNotFoundError) {
        /// ステータスコードの確認
        int statusCode = pReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 404) {
            /// Webページが存在しない場合 (HTTPエラー404)
            pReply->deleteLater();
            return 1;
        }
    }

    // 次に、Webページが存在する場合であっても落ちているスレッドのページが返る時がある
    // そのため、Webページから任意のタグの値 (デフォルトの設定では、スレッドのタイトル名) を取得および比較して、再度、スレッドの生存を確認する
    QString checkElement = "";  // Webページから取得するタグの値

    /// リクエストの作成
    request.setUrl(url);

    /// リダイレクトを有効
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, true);

    /// レスポンスを取得
    pReply = manager.get(request);

    /// レスポンス待機
    QObject::connect(pReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    /// レスポンスの確認
    if (pReply->error() == QNetworkReply::NoError) {
        QString encodedData = "";
        if (shiftjis) {
            /// Shift-JISからUTF-8へデコード
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QStringDecoder decoder("Shift-JIS");
            encodedData = decoder(pReply->readAll());
#else
            auto codec  = QTextCodec::codecForName("Shift-JIS");
            encodedData = codec->toUnicode(pReply->readAll());
#endif

        }
        else {
            encodedData = pReply->readAll();
        }

        xmlDocPtr doc = htmlReadDoc((const xmlChar*)encodedData.toStdString().c_str(), nullptr, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
        if (doc == nullptr) {
            std::cerr << QString("HTMLドキュメントのパースに失敗").toStdString() << std::endl;
            return -1;
        }

        xmlXPathContext* xpathCtx = xmlXPathNewContext(doc);
        if (xpathCtx == nullptr) {
            std::cerr << QString("XPathコンテキストの作成に失敗").toStdString() << std::endl;
            xmlFreeDoc(doc);

            return -1;
        }

        xmlXPathObject* xpathObj = xmlXPathEvalExpression((const xmlChar*)ExpiredXPath.toStdString().c_str(), xpathCtx);
        if (xpathObj == nullptr) {
            std::cerr << QString("XPath式の評価に失敗").toStdString() << std::endl;
            xmlXPathFreeContext(xpathCtx);
            xmlFreeDoc(doc);

            return -1;
        }

        if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
            xmlNode* elementNode = xpathObj->nodesetval->nodeTab[0];
            if (elementNode->children && elementNode->children->content) {
                /// スレッドのタイトル名を取得
                checkElement = reinterpret_cast<const char*>(elementNode->children->content);

                /// 正規表現を定義（スペースを含む [ と ] の間に任意の文字列があるパターン）
                /// (現在は使用しない)
                //static const QRegularExpression RegEx(" \\[.*\\]$");

                /// 文字列からパターンに一致する部分を削除
                /// (現在は使用しない)
                //checkElement = checkElement.remove(RegEx);

                /// 設定ファイルの"expiredelement"キーの値と取得したタグの値を比較
                /// デフォルトの設定では、スレッドタイトル名 (落ちている状態のスレッドタイトル名) を比較
                // if (checkElement.compare(ExpiredElement, Qt::CaseSensitive) == 0) {
                if (checkElement.compare(ExpiredElement, Qt::CaseSensitive) != 0) {
                    /// スレッドが落ちていると判断した場合
                    xmlXPathFreeObject(xpathObj);
                    xmlXPathFreeContext(xpathCtx);
                    xmlFreeDoc(doc);

                    return 1;
                }
            }
        }

        xmlXPathFreeObject(xpathObj);
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);

        pReply->deleteLater();
    }
    else {
        /// レスポンスの取得に失敗した場合
        std::cerr << QString("ネットワークエラー: %1").arg(pReply->errorString()).toStdString() << std::endl;
        pReply->deleteLater();

        return -1;
    }

    return 0;
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
        std::cerr << "エラー : HTMLドキュメントのパースに失敗" << std::endl;
        reply->deleteLater();

        return -1;
    }

    // XPathで特定の要素を検索
    auto *xpath = xmlStrdup((const xmlChar*)_xpath.toUtf8().constData());
    xmlXPathObjectPtr result = getNodeset(doc, xpath);
    if (result == nullptr) {
        std::cerr << "エラー : ノードの取得に失敗" << std::endl;
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
        return nullptr;
    }

    xmlXPathObjectPtr result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == nullptr) {
        return nullptr;
    }

    if (xmlXPathNodeSetIsEmpty(result->nodesetval)) {
        xmlXPathFreeObject(result);
        return nullptr;
    }

    return result;
}


// ニュース記事のURLにアクセスして、XPathで指定した値を取得する
int HtmlFetcher::fetchElement(const QUrl &url, bool redirect, const QString &_xpath, int elementType)
{
    m_Element.clear();

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


int HtmlFetcher::fetchElementJiJiFlashUrl(const QUrl &url, bool redirect, const QString &_xpath, int elementType)
{
    m_Element.clear();

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
        xmlCleanupParser();
        pReply->deleteLater();

        return -1;
    }

    // XPathで特定の要素を検索
    auto *xpath = xmlStrdup((const xmlChar*)_xpath.toUtf8().constData());
    xmlXPathObjectPtr result = getNodeset(doc, xpath);
    if (result == nullptr) {
        xmlFreeDoc(doc);
        xmlCleanupParser();
        pReply->deleteLater();

        return 0;
    }

    // 結果のノードセットからテキストを取得
    auto ret = 0;
    if (getUrl(result->nodesetval, elementType)) {
        ret = -1;
    }

    // libxml2オブジェクトの破棄
    xmlXPathFreeObject(result);
    xmlFreeDoc(doc);

    // libxml2のクリーンアップ
    xmlCleanupParser();

    pReply->deleteLater();

    return ret;
}


// 時事ドットコムの速報記事の"<この速報の記事を読む>"の部分のリンクを取得する
bool HtmlFetcher::getUrl(const xmlNodeSetPtr nodeset, int elementType)
{
    for (auto i = 0; i < nodeset->nodeNr; i++) {
        xmlNodePtr cur = nodeset->nodeTab[i]->xmlChildrenNode;
        while (cur != nullptr) {
            if (cur->type == elementType) {
                QString tagName = (const char*)cur->name;
                if (tagName.compare("a", Qt::CaseSensitive) == 0) {
                    m_Element = (const char*)cur->properties->children->content;
                    return true;
                }
            }
            cur = cur->next;
        }
    }

    return false;
}


// 共同通信の速報記事のURLにアクセスして、XPathで指定した本文を取得する
int HtmlFetcher::fetchParagraphKyodoFlash(const QUrl &url, bool redirect, const QString &_xpath)
{
    m_Element.clear();

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

    // XPathコンテキストの生成
    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    if (context == nullptr) {
        std::cerr << "エラー : XPathコンテキストの生成に失敗" << std::endl;
        xmlFreeDoc(doc);
        pReply->deleteLater();

        return -1;
    }

    // XPath評価
    xmlXPathObjectPtr result = xmlXPathEvalExpression((const xmlChar*)_xpath.toUtf8().constData(), context);
    if (result == nullptr) {
        std::cerr << "エラー : XPath評価に失敗" << std::endl;
        xmlXPathFreeContext(context);
        xmlFreeDoc(doc);
        pReply->deleteLater();

        return -1;
    }

    // 結果のノードセットからテキストを取得
    xmlNodeSetPtr nodes = result->nodesetval;
    if (nodes) {
        for (int i = 0; i < nodes->nodeNr; i++) {
            xmlNodePtr pNode = nodes->nodeTab[i];
            QString paragraphText;

            // pタグの全ての子ノードを処理
            for (xmlNodePtr current = pNode->children; current; current = current->next) {
                if (current->type == XML_TEXT_NODE) {
                    if (current->content) {
                        paragraphText += QString::fromUtf8((const char*)current->content);
                    }
                }
                else if (current->type == XML_ELEMENT_NODE && xmlStrcmp(current->name, BAD_CAST "a") == 0) {
                    xmlNodePtr textNode = current->children;
                    if (textNode && textNode->content) {
                        paragraphText += QString::fromUtf8((const char*)textNode->content);
                    }
                }
            }

            m_Element += paragraphText;
            m_Element  = m_Element.trimmed();
        }
    }

    // libxml2オブジェクトの破棄
    xmlXPathFreeObject(result);
    xmlXPathFreeContext(context);
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

    // 結果のノードセットから最後尾のレス番号を取得
    m_Element.clear();
    xmlNodeSetPtr nodeset = result->nodesetval;

    /// 最後尾のノードセットを取得する
    xmlNodePtr cur = nodeset->nodeTab[nodeset->nodeNr - 1]->xmlChildrenNode;
    if (cur->type == elementType) {
        auto buffer = QString(((const char*)cur->content));
        m_Element.append(buffer);
    }
    else {
        /// XPathで取得したノードセットが最後尾に1つ多く取得される場合があるため、最後尾から1つ前のノードセットを取得する
        cur = nodeset->nodeTab[nodeset->nodeNr - 2]->xmlChildrenNode;
        if (cur->type == elementType) {
            auto buffer = QString(((const char*)cur->content));
            m_Element.append(buffer);
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


// 既存のスレッドからスレッドのタイトルを抽出する
int HtmlFetcher::extractThreadTitle(const QUrl &url, bool redirect, const QString &_xpath, bool bShiftJIS)
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

    // 本文の一部を取得
    if (pReply->error() != QNetworkReply::NoError) {
        // ステータスコードの確認
        int statusCode = pReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 404) {
            // 該当スレッドが存在しない(落ちている)場合
            pReply->deleteLater();
            return 1;
        }

        std::cerr << QString("エラー : %1").arg(pReply->errorString()).toStdString();
        pReply->deleteLater();

        return -1;
    }

    QString htmlContent;
    if (bShiftJIS) {
        // Shift-JISからUTF-8へエンコード
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QStringDecoder decoder("Shift-JIS");
        htmlContent = decoder(pReply->readAll());
#else
        auto codec  = QTextCodec::codecForName("Shift-JIS");
        htmlContent = codec->toUnicode(pReply->readAll());
#endif
    }
    else {
        htmlContent = pReply->readAll();
    }

    // libxml2の初期化
    xmlInitParser();
    LIBXML_TEST_VERSION

    // 文字列からHTMLドキュメントをパース
    // libxml2ではエンコーディングの自動判定において問題があるため、エンコーディングを明示的に指定する
    xmlDocPtr doc = htmlReadDoc((const xmlChar*)htmlContent.toStdString().c_str(), nullptr, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (doc == nullptr) {
        std::cerr << QString("エラー: スレッドURLからHTMLのパースに失敗しました").toStdString() << std::endl;
        pReply->deleteLater();

        return -1;
    }

    // XPathで特定の要素を検索
    auto *xpath = xmlStrdup((const xmlChar*)_xpath.toUtf8().constData());
    xmlXPathObjectPtr result = getNodeset(doc, xpath);
    if (result == nullptr) {
        std::cerr << QString("エラー: スレッドURLからノードの取得に失敗しました").toStdString() << std::endl;
        xmlFreeDoc(doc);
        pReply->deleteLater();

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
                content.append(buffer);
            }
            cur = cur->next;
        }
    }

    // エレメントを取得
    m_Element = content;

    xmlXPathFreeObject(result);
    xmlFreeDoc(doc);
    xmlCleanupParser();

    pReply->deleteLater();

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
