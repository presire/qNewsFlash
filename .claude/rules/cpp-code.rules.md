---
globs: ["*.cpp", "*.h", "*.hpp"]
---

# C++/Qt Code Rules

qNewsFlash プロジェクトの C++/Qt コーディング規約。
このルールは `*.cpp`, `*.h`, `*.hpp` ファイルを編集する際に自動的に適用されます。

---

## 言語標準

- **C++17** を使用
- Qt コーディング規約に従う
- 拡張子: `.cpp` (実装), `.h` (ヘッダ)

---

## 命名規則

### クラス
- **PascalCase** を使用
- 例: `HtmlFetcher`, `WriteMode`, `JiJiFlash`

### メソッド/関数
- **camelCase** を使用
- 例: `fetchHtml()`, `postArticle()`, `convertToShiftJIS()`

### 変数
- **camelCase** を使用
- 例: `newsArticle`, `threadKey`, `networkReply`
- メンバ変数: `m_` プレフィックス (例: `m_manager`)

### 定数
- **UPPER_SNAKE_CASE** を使用
- 例: `MAX_RETRY_COUNT`, `DEFAULT_TIMEOUT`

### Qt Signal/Slot
- **camelCase** を使用
- 例: `void fetched(const QString &html)`

---

## ファイル構成

### ヘッダファイル (.h)
```cpp
#ifndef HTMLFETCHER_H
#define HTMLFETCHER_H

#include <QObject>
#include <QNetworkAccessManager>

class HtmlFetcher : public QObject
{
    Q_OBJECT

public:
    explicit HtmlFetcher(QObject *parent = nullptr);
    ~HtmlFetcher();

    void fetchUrl(const QString &url);

signals:
    void fetched(const QString &html);
    void error(const QString &message);

private:
    QNetworkAccessManager *m_manager;
};

#endif // HTMLFETCHER_H
```

### 実装ファイル (.cpp)
```cpp
#include "HtmlFetcher.h"
#include <QNetworkReply>

HtmlFetcher::HtmlFetcher(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

HtmlFetcher::~HtmlFetcher()
{
}

void HtmlFetcher::fetchUrl(const QString &url)
{
    // 実装
}
```

---

## 必須事項

### Q_OBJECT マクロ
- Signal/Slot を使用するクラスでは必須
- クラス宣言の先頭に配置

```cpp
class MyClass : public QObject
{
    Q_OBJECT  // 必須

public:
    // ...
};
```

### メモリ管理
- QObject 親子関係を活用
- `deleteLater()` で非同期削除
- Smart Pointer を優先 (`QSharedPointer`, `QScopedPointer`)

```cpp
// OK: 親を設定
QNetworkAccessManager *manager = new QNetworkAccessManager(this);

// OK: deleteLater
reply->deleteLater();

// OK: Smart Pointer
QScopedPointer<xmlDoc, QScopedPointerPodDeleter> doc(xmlParseMemory(...));
```

---

## 禁止事項

### 型安全性
```cpp
// 禁止: as any キャスト
QObject *obj = dynamic_cast<QObject*>(something);

// 禁止: @ts-ignore に相当する警告抑制
// コメントで警告を無視しない
```

### 例外処理
```cpp
// 禁止: 例外を使用しない (Qtスタイル)
try {
    // ...
} catch (...) {
    // 禁止
}

// OK: エラーコードまたは bool で処理
bool success = doSomething();
if (!success) {
    qWarning() << "Operation failed";
    return false;
}
```

### 空の catch ブロック
```cpp
// 禁止
catch (...) {}  // 絶対に使用しない
```

---

## ログ出力

```cpp
#include <QDebug>

// デバッグ情報
qDebug() << "Fetching URL:" << url;

// 警告
qWarning() << "Network error:" << reply->errorString();

// 重大なエラー
qCritical() << "Failed to initialize";
```

---

## エラーハンドリング

### ネットワークエラー
```cpp
QNetworkReply *reply = manager->get(request);
connect(reply, &QNetworkReply::finished, [reply]() {
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Error:" << reply->errorString();
        reply->deleteLater();
        return;
    }
    // 成功時の処理
    reply->deleteLater();
});
```

### nullptr チェック
```cpp
if (!ptr) {
    qWarning() << "Null pointer";
    return false;
}
```

---

## Qt5/Qt6 互換性

```cpp
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6 固有のコード
#else
    // Qt5 固有のコード
#endif
```

### 主な違い
- `QTextCodec` → Qt6 では `QStringConverter`
- `QRegExp` → Qt6 では `QRegularExpression`
- `QNetworkConfigurationManager` → Qt6 で変更

---

## コメント

- 日本語コメント可 (UTF-8)
- Doxygen 形式を推奨

```cpp
/**
 * @brief HTMLを取得する
 * @param url 取得先URL
 * @return 成功したらtrue
 */
bool fetchHtml(const QString &url);
```

---

## インクルード順序

1. 対応するヘッダ (例: `HtmlFetcher.h`)
2. Qt ヘッダ
3. 標準ライブラリ
4. サードパーティ (libxml2 等)
5. プロジェクト内の他ヘッダ

```cpp
#include "HtmlFetcher.h"       // 1. 対応するヘッダ

#include <QObject>             // 2. Qt
#include <QNetworkReply>

#include <memory>              // 3. 標準ライブラリ

#include <libxml/parser.h>     // 4. サードパーティ

#include "Article.h"           // 5. プロジェクト内
```
