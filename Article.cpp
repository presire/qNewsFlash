#include "Article.h"


Article::Article(QObject *parent) : QObject{parent}
{

}


Article::Article(const Article &obj, QObject *parent) noexcept : QObject{parent}
{
    //std::tie(this->m_Title, this->m_Paragraph, this->m_URL, this->m_Date) = other.getArticleData();
    auto [title, paragraph, url, date] = obj.getArticleData();

    this->m_Title     = title;
    this->m_Paragraph = paragraph;
    this->m_URL       = url;
    this->m_Date      = date;
}


Article::Article(QString title, QString paragraph, QString url, QString date, QObject *parent) :
    m_Title(title), m_Paragraph(paragraph), m_URL(url), m_Date(date), QObject{parent}
{

}


Article::~Article()
{

}


Article& Article::operator=(const Article &other)
{
    if (this != &other) {
        // リソースをコピー
        //std::tie(this->m_Title, this->m_Paragraph, this->m_URL, this->m_Date) = other.getArticleData();
        auto [title, paragraph, url, date] = other.getArticleData();

        this->m_Title     = title;
        this->m_Paragraph = paragraph;
        this->m_URL       = url;
        this->m_Date      = date;
    }

    // このオブジェクトの参照を返す
    return *this;
}


std::tuple<QString, QString, QString, QString> Article::getArticleData() const
{
    return std::make_tuple(m_Title, m_Paragraph, m_URL, m_Date);
}
