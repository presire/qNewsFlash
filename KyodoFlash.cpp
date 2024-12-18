#include <libxml/parser.h>
#include <libxml/tree.h>
#include <iostream>
#include "KyodoFlash.h"
#include "HtmlFetcher.h"


KyodoFlash::KyodoFlash(long long maxPara, KYODOFLASHINFO Info, QObject *parent) :
    m_MaxParagraph(maxPara), m_FlashInfo(Info), QObject{parent}
{

}


KyodoFlash::~KyodoFlash() = default;


int KyodoFlash::FetchFlash()
{
    HtmlFetcher fetcher(this);

    // 速報記事の一覧が記載されているURLにアクセスして速報記事のURLを取得
    if (fetcher.fetchElement(m_FlashInfo.FlashUrl, true, m_FlashInfo.FlashXPath, XML_TEXT_NODE)) {
        std::cerr << QString("エラー : (共同通信) 速報記事の取得に失敗").toStdString() << std::endl;
        return -1;
    }

    /// 速報記事のURLを取得 (/xxxx.html形式)
    auto articleLink = fetcher.GetElement();

    /// 末尾の半角スペースを削除
    if (articleLink.endsWith(" ")) articleLink.chop(1);

    /// 速報記事の基準となるURLと上記で取得した速報記事のURLを結合
    auto link = m_FlashInfo.BasisURL + articleLink;

    // 速報記事のURLにアクセスして速報記事のタイトル名を取得
    if (fetcher.fetchElement(link, true, m_FlashInfo.TitleXPath, XML_TEXT_NODE)) {
        std::cerr << QString("エラー : (共同通信) 速報記事のタイトルの取得に失敗").toStdString() << std::endl;
        return -1;
    }

    /// 速報記事のタイトルを取得
    auto title = fetcher.GetElement();

    /// 末尾の半角スペースを削除
    if (title.endsWith(" ")) title.chop(1);

    // 速報記事のURLにアクセスして速報記事の本文を取得
    // 現在、速報記事の本文の取得に失敗した場合でもエラーとしない
    // if (fetcher.fetchParagraphKyodoFlash(link, true, m_FlashInfo.ParaXPath)) {
    //     std::cerr << QString("エラー : (共同通信) 速報記事の本文の取得に失敗").toStdString() << std::endl;
    //     return -1;
    // }

    // /// 速報記事の本文を取得
    // auto paragraph = fetcher.GetElement();

    [[__maybe_unused__]] auto iRet = fetcher.fetchParagraphKyodoFlash(link, true, m_FlashInfo.ParaXPath);

    /// 速報記事の本文を取得
    /// 現在の仕様では、本文の取得に失敗した場合は空欄とする
    auto paragraph = iRet == 0 ? fetcher.GetElement() : "";

    /// 末尾の半角スペースを削除
    if (paragraph.endsWith(" ")) paragraph.chop(1);

    /// 本文が指定文字数以上の場合、指定文字数のみを抽出
    paragraph = paragraph.size() > m_MaxParagraph ? paragraph.mid(0, static_cast<int>(m_MaxParagraph)) + QString("...") : paragraph;

    // 速報記事のURLにアクセスして速報記事の公開日を取得
    if (fetcher.fetchElement(link, true, m_FlashInfo.PubDateXPath, XML_TEXT_NODE)) {
        std::cerr << QString("エラー : (共同通信) 速報記事の公開日の取得に失敗").toStdString() << std::endl;
        return -1;
    }

    /// 速報記事の本文を取得
    auto date = fetcher.GetElement();
    if (date.isEmpty()) {
        std::cerr << QString("エラー : (共同通信) 速報記事の公開日の取得に失敗").toStdString() << std::endl;
        return -1;
    }

    /// 末尾の半角スペースを削除
    if (date.endsWith(" ")) date.chop(1);

    /// 共同通信の日付形式を"yyyy年M月d日 H時m分"に変換
    date = convertDate(date);

    m_Title     = title;
    m_Paragraph = paragraph;
    m_URL       = link;
    m_Date      = date;

    return 0;
}


// 共同通信の日付形式を"yyyy年M月d日 H時m分"に変換
QString KyodoFlash::convertDate(QString &strDate)
{
    // 共同通信の日時列を解析
    auto dateTime = QDateTime::fromString(strDate, "yyyy年MM月dd日 HH時mm分");
    QString convertDate = "";

    if (!dateTime.isValid()) {
        std::cerr << QString("日付の変換に失敗: %1").arg(strDate).toStdString();
    }
    else {
        // "yyyy年M月d日 H時m分" 形式に変換
        convertDate = dateTime.toString("yyyy年M月d日 H時m分");
    }

    return convertDate;
}


// フォーマットに合わせた速報記事を取得する
std::tuple<QString, QString, QString, QString> KyodoFlash::getArticleData() const
{
    return std::make_tuple(m_Title, m_Paragraph, m_URL, m_Date);
}
