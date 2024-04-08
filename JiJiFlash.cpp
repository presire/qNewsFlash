#include <libxml/parser.h>
#include <libxml/tree.h>
#include <iostream>
#include "JiJiFlash.h"
#include "HtmlFetcher.h"


JiJiFlash::JiJiFlash(long long maxPara, JIJIFLASHINFO Info, QObject *parent) :
    m_MaxParagraph(maxPara), m_FlashInfo(Info), QObject{parent}
{

}


JiJiFlash::~JiJiFlash() = default;


int JiJiFlash::FetchFlash()
{
    HtmlFetcher fetcher(this);

    // 速報記事の一覧が記載されているURLにアクセスして速報記事のURLを取得
    if (fetcher.fetchElement(m_FlashInfo.FlashUrl, true, m_FlashInfo.FlashXPath, XML_TEXT_NODE)) {
        std::cerr << QString("エラー : 時事ドットコムの速報記事の取得に失敗").toStdString() << std::endl;
        return -1;
    }

    /// 速報記事のURLを取得
    auto articleLink = fetcher.GetElement();

    /// 末尾の半角スペースを削除
    if (articleLink.endsWith(" ")) articleLink.chop(1);

    /// 速報記事の基準となるURLと上記で取得した速報記事のURLを結合
    auto link = m_FlashInfo.BasisURL + articleLink;

    // 速報記事のURLにアクセスして速報記事のタイトル名を取得
    if (fetcher.fetchElement(link, true, m_FlashInfo.TitleXPath, XML_TEXT_NODE)) {
        std::cerr << QString("エラー : 速報記事のタイトルの取得に失敗").toStdString() << std::endl;
        return -1;
    }

    /// 速報記事のタイトルを取得
    auto title = fetcher.GetElement();

    /// 末尾の半角スペースを削除
    if (title.endsWith(" ")) title.chop(1);

    // 速報記事のURLにアクセスして速報記事の本文を取得
    if (fetcher.fetchElement(link, true, m_FlashInfo.ParaXPath, XML_TEXT_NODE)) {
        std::cerr << QString("エラー : 速報記事の本文の取得に失敗").toStdString() << std::endl;
        return -1;
    }

    /// 速報記事の本文を取得
    auto paragraph = fetcher.GetElement();

    /// 末尾の半角スペースを削除
    if (paragraph.endsWith(" ")) paragraph.chop(1);

    /// 本文が指定文字数以上の場合、指定文字数のみを抽出
    paragraph = paragraph.size() > m_MaxParagraph ? paragraph.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : paragraph;

    // 速報記事のURLにアクセスして速報記事の公開日を取得
    if (fetcher.fetchElement(link, true, m_FlashInfo.PubDateXPath, XML_TEXT_NODE)) {
        std::cerr << QString("エラー : 速報記事の公開日の取得に失敗").toStdString() << std::endl;
        return -1;
    }

    /// 速報記事の本文を取得
    auto date = fetcher.GetElement();

    /// 末尾の半角スペースを削除
    if (date.endsWith(" ")) date.chop(1);

    /// 日付をISO8601形式から"yyyy年M月d日 H時m分"に変換
    date = convertDate(date);

    // 速報記事のURLにアクセスして"<この速報の記事を読む>"の部分のリンクを取得
    // このリンクが存在しない場合は速報記事とする
    // このリンクが存在する場合は、既に本記事があるため速報記事と看做さない
    if (fetcher.fetchElementJiJiFlashUrl(link, true, m_FlashInfo.UrlXPath, XML_ELEMENT_NODE)) {
        // 既に本記事が存在する場合、または、読み込みエラーの場合
        return -1;
    }

    /// 速報記事の"<この速報の記事を読む>"の部分のリンクを取得
    auto origLink = fetcher.GetElement();

    /// リンクが存在するかどうかを確認
    if (!origLink.isEmpty()) {
        /// リンクが存在する場合
        return -1;
    }

    m_Title     = title;
    m_Paragraph = paragraph;
    m_URL       = link;
    m_Date      = date;

    return 0;
}


// 日付をISO8601形式から"yyyy年M月d日 H時m分"に変換
QString JiJiFlash::convertDate(QString &strDate)
{
    // ISO 8601形式 (YYYY-MM-DDTHH:mm:SS+XX:XX) の日時列を解析
    auto dateTime = QDateTime::fromString(strDate, Qt::ISODate);
    QString convertDate = "";

    if (!dateTime.isValid()) {
        std::cerr << QString("日付の変換に失敗 (時事速報) : %1").arg(strDate).toStdString();
    }
    else {
        convertDate = dateTime.toString("yyyy年M月d日 H時m分");
    }

    return convertDate;
}


// フォーマットに合わせた速報記事を取得する
std::tuple<QString, QString, QString, QString> JiJiFlash::getArticleData() const
{
    return std::make_tuple(m_Title, m_Paragraph, m_URL, m_Date);
}
