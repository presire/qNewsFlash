#ifndef ARTICLE_H
#define ARTICLE_H

#include <QObject>
#include <tuple>


class Article : public QObject
{
    Q_OBJECT

private:
    QString m_Title;
    QString m_Paragraph;
    QString m_URL;
    QString m_Date;

public:
    Article() = default;
    explicit Article(QObject *parent = nullptr);
    Article(const Article &obj, QObject *parent = nullptr) noexcept;
    Article(QString, QString, QString, QString, QObject *parent = nullptr);
     ~Article() override;

    Article& operator=(const Article &other);  // 代入演算子のオーバーロード
    [[nodiscard]] std::tuple<QString, QString, QString, QString> getArticleData() const;

signals:

};

#endif // ARTICLE_H
