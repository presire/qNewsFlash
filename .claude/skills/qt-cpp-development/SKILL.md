# Qt C++ Development Skill

qNewsFlash プロジェクトのための Qt5/Qt6 C++ 開発スキル。
ネットワーク通信、文字列処理、メモリ管理、非同期処理のベストプラクティスを提供。

---

## 概要

このスキルは、qNewsFlash プロジェクトで使用される Qt Framework の
主要コンポーネントと実装パターンをカバーします。

**対象 Qt バージョン**: Qt 5.15+ / Qt 6.x

---

## 主要コンポーネント

### Qt Network

#### 基本的な HTTP GET

```cpp
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

class HtmlFetcher : public QObject {
    Q_OBJECT
public:
    explicit HtmlFetcher(QObject *parent = nullptr) : QObject(parent) {
        m_manager = new QNetworkAccessManager(this);
    }
    
    void fetchUrl(const QString &url);
    
signals:
    void fetched(const QString &html, const QString &url);
    void error(const QString &errorMsg);
    
private:
    QNetworkAccessManager *m_manager;
};

void HtmlFetcher::fetchUrl(const QString &url) {
    QNetworkRequest request(QUrl(url));
    request.setHeader(QNetworkRequest::UserAgentHeader, "qNewsFlash/1.0");
    request.setRawHeader("Accept", "text/html,application/xhtml+xml");
    
    // Qt 6: HTTP/2 がデフォルトで有効
    // 無効化する場合: request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    
    QNetworkReply *reply = m_manager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit error(reply->errorString());
            reply->deleteLater();
            return;
        }
        
        QString html = QString::fromUtf8(reply->readAll());
        emit fetched(html, url);
        reply->deleteLater();
    });
    
    // Qt 6: SSL/TLS ハンドシェイク完了シグナル
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(reply, &QNetworkReply::encrypted, this, []() {
        qDebug() << "SSL/TLS handshake completed";
    });
#endif
}
```

#### POST 送信 (掲示板書き込み用)

```cpp
void postToBoard(const QString &url, const QByteArray &postData) {
    QNetworkRequest request(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, 
                      "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::ContentLengthHeader, postData.size());
    
    QNetworkReply *reply = m_manager->post(request, postData);
    
    connect(reply, &QNetworkReply::finished, [reply]() {
        qDebug() << "POST response:" << reply->readAll();
        reply->deleteLater();
    });
}
```

#### SSL エラー処理

```cpp
connect(reply, &QNetworkReply::sslErrors, [](const QList<QSslError> &errors) {
    for (const auto &error : errors) {
        qWarning() << "SSL Error:" << error.errorString();
    }
    // 開発環境でのみ使用 (本番では回避すること)
    // reply->ignoreSslErrors();
});
```

#### ネットワークタイムアウト実装

```cpp
class NetworkHelper : public QObject {
    Q_OBJECT
public:
    static void setTimeout(QNetworkReply *reply, int timeoutMs) {
        QTimer *timer = new QTimer(reply);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, reply, [reply]() {
            qWarning() << "Request timeout, aborting:" << reply->url();
            reply->abort();
        });
        timer->start(timeoutMs);
    }
};

// 使用例
QNetworkReply *reply = m_manager->get(request);
NetworkHelper::setTimeout(reply, 30000); // 30秒タイムアウト
```

---

### Qt Core (文字列・エンコーディング)

#### Qt 5: QTextCodec による変換

```cpp
#include <QTextCodec>
#include <QString>

// Qt 5 での Shift-JIS 変換
QByteArray convertToShiftJIS_Qt5(const QString &text) {
    QTextCodec *codec = QTextCodec::codecForName("Shift-JIS");
    if (!codec) {
        qWarning() << "Shift-JIS codec not available";
        return QByteArray();
    }
    return codec->fromUnicode(text);
}

QString convertFromShiftJIS_Qt5(const QByteArray &data) {
    QTextCodec *codec = QTextCodec::codecForName("Shift-JIS");
    return codec ? codec->toUnicode(data) : QString();
}
```

#### Qt 6: QStringEncoder/QStringDecoder

Qt 6 では `QTextCodec` が廃止され、`QStringEncoder`/`QStringDecoder` に置き換えられました。

```cpp
#include <QStringEncoder>
#include <QStringDecoder>

// Qt 6 での Shift-JIS エンコード
QByteArray convertToShiftJIS_Qt6(const QString &text) {
    QStringEncoder encoder(QStringEncoder::Encoding::Utf8);
    // Shift-JIS は名前で指定
    encoder = QStringEncoder("Shift-JIS");
    
    if (!encoder.isValid()) {
        qWarning() << "Invalid encoding:" << encoder.name();
        return QByteArray();
    }
    
    QByteArray result = encoder.encode(text);
    if (encoder.hasError()) {
        qWarning() << "Encoding error occurred";
    }
    return result;
}

// Qt 6 での Shift-JIS デコード
QString convertFromShiftJIS_Qt6(const QByteArray &data) {
    QStringDecoder decoder("Shift-JIS");
    if (!decoder.isValid()) {
        qWarning() << "Invalid encoding";
        return QString();
    }
    return decoder.decode(data);
}
```

#### Qt 6 対応の Encoding 列挙型

```cpp
// QStringEncoder/QStringDecoder で使用可能なエンコーディング
enum class Encoding {
    Utf8,
    Utf16,
    Utf16LE,
    Utf16BE,
    Utf32,
    Utf32LE,
    Utf32BE,
    Latin1,
    System      // ロケール依存
};

// 使用例
QStringEncoder utf8Encoder(QStringEncoder::Encoding::Utf8);
QStringEncoder shiftJisEncoder("Shift-JIS");  // 名前で指定
QStringEncoder eucJpEncoder("EUC-JP");
QStringEncoder iso2022JpEncoder("ISO-2022-JP");
```

#### Qt 5/Qt6 互換コード

```cpp
#include <QtGlobal>

QByteArray toShiftJIS(const QString &text) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QStringEncoder encoder("Shift-JIS");
    return encoder.isValid() ? encoder.encode(text) : QByteArray();
#else
    QTextCodec *codec = QTextCodec::codecForName("Shift-JIS");
    return codec ? codec->fromUnicode(text) : QByteArray();
#endif
}

QString fromShiftJIS(const QByteArray &data) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QStringDecoder decoder("Shift-JIS");
    return decoder.isValid() ? decoder.decode(data) : QString();
#else
    QTextCodec *codec = QTextCodec::codecForName("Shift-JIS");
    return codec ? codec->toUnicode(data) : QString();
#endif
}
```

---

### Qt JSON

#### 設定ファイル読み込み

```cpp
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

QJsonObject loadConfig(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open config:" << path << file.errorString();
        return QJsonObject();
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << parseError.errorString();
        return QJsonObject();
    }
    
    return doc.object();
}
```

#### 設定値の安全な取得

```cpp
// ネストした設定の取得例
QString getXPath(const QJsonObject &config, const QString &section, const QString &key) {
    QJsonObject sectionObj = config.value(section).toObject();
    return sectionObj.value(key).toString();
}

// デフォルト値付き
QString getXPathWithDefault(const QJsonObject &config, const QString &section, 
                            const QString &key, const QString &defaultValue) {
    QJsonObject sectionObj = config.value(section).toObject();
    if (sectionObj.contains(key)) {
        return sectionObj.value(key).toString();
    }
    return defaultValue;
}

// 使用例
QJsonObject config = loadConfig("/etc/qNewsFlash/qNewsFlash.json");
QString flashXpath = getXPath(config, "jijiflash", "flashxpath");
QString titleXpath = getXPathWithDefault(config, "jijiflash", "titlexpath", "//title");
```

#### 設定ファイル書き込み

```cpp
bool saveConfig(const QString &path, const QJsonObject &config) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot write config:" << path << file.errorString();
        return false;
    }
    
    QJsonDocument doc(config);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

// 使用例: threadKey の更新
void updateThreadKey(const QString &configPath, const QString &newKey) {
    QJsonObject config = loadConfig(configPath);
    QJsonObject threadObj = config["thread"].toObject();
    threadObj["threadKey"] = newKey;
    config["thread"] = threadObj;
    saveConfig(configPath, config);
}
```

---

### Qt Timer

#### 定期実行処理

```cpp
#include <QTimer>

class Runner : public QObject {
    Q_OBJECT
public:
    explicit Runner(QObject *parent = nullptr) : QObject(parent) {}
    
    void start(int intervalMs) {
        // 初回は即座に実行
        QTimer::singleShot(0, this, &Runner::fetchNews);
        
        // 定期実行タイマー開始
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &Runner::fetchNews);
        m_timer->start(intervalMs);
    }
    
    void stop() {
        if (m_timer) {
            m_timer->stop();
        }
    }
    
private slots:
    void fetchNews() {
        qDebug() << "Fetching news at" << QDateTime::currentDateTime();
        // ニュース取得処理
    }
    
private:
    QTimer *m_timer = nullptr;
};
```

#### 遅延実行

```cpp
// 1回限りの遅延実行
QTimer::singleShot(5000, this, []() {
    qDebug() << "Executed after 5 seconds";
});

// Lambda を使用した遅延実行
void scheduleRetry(int delayMs, std::function<void()> callback) {
    QTimer::singleShot(delayMs, callback);
}
```

---

## メモリ管理パターン

### QObject 親子関係

```cpp
// 親を設定すると自動削除
class MyClass : public QObject {
    Q_OBJECT
public:
    MyClass(QObject *parent = nullptr) : QObject(parent) {
        // 親が this のため、MyClass 削除時に自動削除される
        m_manager = new QNetworkAccessManager(this);
        m_timer = new QTimer(this);
    }
private:
    QNetworkAccessManager *m_manager;
    QTimer *m_timer;
};
```

### Smart Pointers

```cpp
#include <QSharedPointer>
#include <QScopedPointer>

// 共有所有権
QSharedPointer<Article> article = QSharedPointer<Article>::create();
article->setTitle("Breaking News");

// カスタムデリータ (libxml2 用)
QScopedPointer<xmlDoc, QScopedPointerPodDeleter> doc(xmlParseMemory(data, size));
if (!doc) {
    qWarning() << "Failed to parse XML";
    return;
}
// スコープ終了時に xmlFreeDoc() が呼ばれる

// QObject 用カスタムデリータ
QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(manager->get(request));
```

### deleteLater パターン

```cpp
// イベントループ内で安全に削除
void handleReply(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return;
    }
    // 処理完了後
    reply->deleteLater();
}
```

---

## Signal/Slot パターン

### Lambda 接続

```cpp
// 基本的な Lambda 接続
connect(m_timer, &QTimer::timeout, this, [this]() {
    fetchNews();
});

// 外部変数のキャプチャ
QString baseUrl = "https://example.com";
connect(reply, &QNetworkReply::finished, [baseUrl, reply]() {
    qDebug() << "Request to" << baseUrl << "finished";
    reply->deleteLater();
});
```

### 複数シグナル

```cpp
// 完了とエラーの両方を処理
connect(reply, &QNetworkReply::finished, this, &MyClass::onFinished);

// Qt 5: エラーシグナル (Qt 6 でシグナルの形式が変更)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
        this, &MyClass::onError);
#else
// Qt 6: errorOccurred シグナル
connect(reply, &QNetworkReply::errorOccurred, this, &MyClass::onError);
#endif
```

### シグナルの接続タイプ

```cpp
// 自動接続 (スレッド間なら QueuedConnection)
connect(sender, &Sender::signal, receiver, &Receiver::slot);

// 即座に実行 (同スレッドのみ)
connect(sender, &Sender::signal, receiver, &Receiver::slot, Qt::DirectConnection);

// イベントループ経由 (スレッド間通信)
connect(sender, &Sender::signal, receiver, &Receiver::slot, Qt::QueuedConnection);

// 一度だけ接続
connect(sender, &Sender::signal, receiver, &Receiver::slot, Qt::SingleShotConnection);
```

---

## エラーハンドリング

### ネットワークエラー

```cpp
void handleNetworkReply(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        switch (reply->error()) {
        case QNetworkReply::ConnectionRefusedError:
            qWarning() << "Connection refused";
            break;
        case QNetworkReply::HostNotFoundError:
            qWarning() << "Host not found";
            break;
        case QNetworkReply::TimeoutError:
            qWarning() << "Request timeout";
            break;
        case QNetworkReply::SslHandshakeFailedError:
            qWarning() << "SSL handshake failed";
            break;
        default:
            qWarning() << "Network error:" << reply->errorString();
        }
        reply->deleteLater();
        return;
    }
    
    // 成功時の処理
    QByteArray data = reply->readAll();
    reply->deleteLater();
}
```

### リトライパターン

```cpp
class RetryHelper : public QObject {
    Q_OBJECT
public:
    void fetchWithRetry(const QString &url, int maxRetries, int retryDelayMs) {
        m_url = url;
        m_maxRetries = maxRetries;
        m_retryDelayMs = retryDelayMs;
        m_currentRetry = 0;
        doFetch();
    }
    
private:
    void doFetch() {
        QNetworkReply *reply = m_manager->get(QNetworkRequest(QUrl(m_url)));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            if (reply->error() == QNetworkReply::NoError) {
                emit success(reply->readAll());
                reply->deleteLater();
                return;
            }
            
            reply->deleteLater();
            m_currentRetry++;
            
            if (m_currentRetry < m_maxRetries) {
                qWarning() << "Retry" << m_currentRetry << "/" << m_maxRetries;
                QTimer::singleShot(m_retryDelayMs, this, &RetryHelper::doFetch);
            } else {
                emit failure("Max retries exceeded");
            }
        });
    }
    
    QNetworkAccessManager m_manager;
    QString m_url;
    int m_maxRetries = 3;
    int m_retryDelayMs = 5000;
    int m_currentRetry = 0;
    
signals:
    void success(const QByteArray &data);
    void failure(const QString &error);
};
```

---

## Qt5/Qt6 互換性

### バージョンチェック

```cpp
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt 6 以降のコード
#elif QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    // Qt 5.15 以降のコード
#else
    // Qt 5.15 未満のコード
#endif
```

### 主な移行ポイント

| Qt 5                       | Qt 6                           |
|----------------------------|--------------------------------|
| `QTextCodec`               | `QStringEncoder/QStringDecoder`|
| `QStringRef`               | `QStringView`                  |
| `QRegExp`                  | `QRegularExpression`           |
| `QString::splitRef()`      | `QStringView::split()`         |
| `reply->error()` (signal)  | `reply->errorOccurred()`       |
| `QWheelEvent::delta()`     | `QWheelEvent::angleDelta()`    |

### QStringRef → QStringView 移行

```cpp
// Qt 5
QStringRef ref = string.midRef(0, 10);
QStringList parts = string.splitRef(' ');

// Qt 6
QStringView view = QStringView(string).mid(0, 10);
// または
auto parts = QStringView(string).split(u' ');
```

---

## 非同期処理パターン

### QtConcurrent (並列処理)

```cpp
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

// 複数URLを並列取得
void fetchMultipleUrls(const QStringList &urls) {
    QFuture<QByteArray> future = QtConcurrent::mapped(urls, [](const QString &url) {
        QNetworkAccessManager manager;
        QNetworkReply *reply = manager.get(QNetworkRequest(QUrl(url)));
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        QByteArray data = reply->readAll();
        reply->deleteLater();
        return data;
    });
    
    // 結果の監視
    QFutureWatcher<QByteArray> *watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcher<QByteArray>::finished, [watcher]() {
        QList<QByteArray> results = watcher->results();
        qDebug() << "Fetched" << results.size() << "pages";
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}
```

### QFuture/QPromise (Qt 6)

```cpp
#include <QFuture>
#include <QPromise>

QFuture<QString> fetchTitleAsync(const QString &url) {
    return QtConcurrent::run([url]() {
        QNetworkAccessManager manager;
        QNetworkReply *reply = manager.get(QNetworkRequest(QUrl(url)));
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        
        QString title;
        if (reply->error() == QNetworkReply::NoError) {
            QString html = QString::fromUtf8(reply->readAll());
            QRegularExpression re("<title>(.*?)</title>");
            QRegularExpressionMatch match = re.match(html);
            if (match.hasMatch()) {
                title = match.captured(1);
            }
        }
        reply->deleteLater();
        return title;
    });
}
```

---

## ログ出力

### 基本的なログ

```cpp
#include <QDebug>

qDebug() << "Debug message";
qWarning() << "Warning message";
qCritical() << "Critical message";
qInfo() << "Info message";
```

### カテゴリ付きログ

```cpp
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcNetwork, "qnewsflash.network")
Q_LOGGING_CATEGORY(lcXPath, "qnewsflash.xpath")
Q_LOGGING_CATEGORY(lcPost, "qnewsflash.post")

// 使用例
qCDebug(lcNetwork) << "Fetching URL:" << url;
qCWarning(lcNetwork) << "Retry attempt:" << retryCount;
qCCritical(lcNetwork) << "Connection failed:" << error;

// 環境変数でカテゴリ制御
// QT_LOGGING_RULES="qnewsflash.network=true"
// QT_LOGGING_RULES="qnewsflash.*.debug=false"
```

---

## プロジェクト固有パターン

### Article クラス

```cpp
class Article {
public:
    QString title() const { return m_title; }
    void setTitle(const QString &title) { m_title = title; }
    
    QString url() const { return m_url; }
    void setUrl(const QString &url) { m_url = url; }
    
    QString content() const { return m_content; }
    void setContent(const QString &content) { m_content = content; }
    
    QDateTime publishedAt() const { return m_publishedAt; }
    void setPublishedAt(const QDateTime &dt) { m_publishedAt = dt; }
    
private:
    QString m_title;
    QString m_url;
    QString m_content;
    QDateTime m_publishedAt;
};
```

### HtmlFetcher 完全実装パターン

```cpp
class HtmlFetcher : public QObject {
    Q_OBJECT
public:
    explicit HtmlFetcher(QObject *parent = nullptr);
    
    void fetch(const QString &url);
    void fetchWithRetry(const QString &url, int maxRetries = 3);
    
signals:
    void finished(const QString &html, const QString &url);
    void error(const QString &errorMsg, const QString &url);
    
private slots:
    void onFinished();
    
private:
    QNetworkAccessManager *m_manager;
    QMap<QNetworkReply*, QString> m_pendingUrls;
};
```

### Poster クラス (Shift-JIS 投稿)

```cpp
class Poster : public QObject {
    Q_OBJECT
public:
    explicit Poster(QObject *parent = nullptr);
    
    void post(const QString &url, const QMap<QString, QString> &params);
    
private:
    QByteArray buildPostData(const QMap<QString, QString> &params);
    QByteArray convertToShiftJIS(const QString &text);
    QString escapeUnsupportedChars(const QString &text);
    
signals:
    void posted(const QString &response);
    void error(const QString &errorMsg);
};
```

---

## 参考リンク

- [Qt Documentation](https://doc.qt.io/)
- [Qt Network](https://doc.qt.io/qt-6/qtnetwork-index.html)
- [Qt Core](https://doc.qt.io/qt-6/qtcore-index.html)
- [Qt 6 Porting Guide](https://doc.qt.io/qt-6/portingguide.html)
- [QStringEncoder/Decoder](https://doc.qt.io/qt-6/qstringencoder.html)
- [Qt Concurrent](https://doc.qt.io/qt-6/qtconcurrent-index.html)
