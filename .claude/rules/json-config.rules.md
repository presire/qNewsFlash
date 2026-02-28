---
globs: ["*.json", "*.json.in"]
---

# JSON Config Rules

qNewsFlash プロジェクトの JSON 設定ファイル規約。
このルールは `*.json` および `*.json.in` ファイルを編集する際に自動的に適用されます。

---

## 基本構造

### メイン設定ファイル
```json
{
  "newsapi": { ... },
  "jiji": { ... },
  "kyodo": { ... },
  "jijiflash": { ... },
  "kyodoflash": { ... },
  "thread": { ... },
  "threadcommand": { ... }
}
```

---

## ニュースソース設定

### RSS フィード設定
```json
{
  "newsapi": {
    "url": "https://news.example.com/feed",
    "titlexpath": "//item/title/text()",
    "linkxpath": "//item/link/text()",
    "datexpath": "//item/pubDate/text()"
  }
}
```

### 速報ニュース設定
```json
{
  "jijiflash": {
    "url": "https://www.jiji.com/news/flash/",
    "flashxpath": "//ul[@class='newsList']/li",
    "titlexpath": ".//a/text()",
    "urlxpath": ".//a/@href",
    "datexpath": ".//span[@class='date']/text()",
    "paraxpath": "/html/head/meta[@name='description']/@content"
  }
}
```

---

## 掲示板設定

### スレッド設定
```json
{
  "thread": {
    "server": "https://example.0ch.net",
    "board": "news",
    "threadkey": "1234567890",
    "writemode": 1,
    "maxarticles": 100
  }
}
```

### 書き込みモード
| 値 | モード | 説明 |
|----|--------|------|
| 1 | 単一スレッド | 指定スレッドに追記 |
| 2 | 常時新規スレッド | 毎回新規スレッド作成 |
| 3 | ハイブリッド | スレッドが満杯なら新規作成 |

---

## コマンド設定

### スレッドコマンド
```json
{
  "threadcommand": {
    "!chtt": {
      "enabled": true,
      "template": "【速報】${title}\n\n${url}\n${description}"
    },
    "!hogo": {
      "enabled": true,
      "template": "【補足】${content}"
    },
    "!bottom": {
      "enabled": false,
      "template": ""
    }
  }
}
```

---

## テンプレート変数

| 変数 | 説明 | 例 |
|------|------|-----|
| `${title}` | 記事タイトル | "速報：ニュースタイトル" |
| `${url}` | 記事URL | "https://..." |
| `${description}` | 記事概要 | "記事の要約..." |
| `${date}` | 日時 | "2024/01/01 12:00" |
| `${content}` | 記事本文 | "詳細な内容..." |

---

## 型定義

### 必須フィールド
| フィールド | 型 | 必須 | 説明 |
|------------|-----|------|------|
| `url` | string | ✓ | URL |
| `flashxpath` | string | ✓ | 一覧取得XPath |
| `titlexpath` | string | ✓ | タイトルXPath |
| `urlxpath` | string | ✓ | URL XPath |
| `datexpath` | string | | 日時XPath |
| `paraxpath` | string | | 本文XPath |

---

## 設定ファイルテンプレート (.json.in)

### 変数置換
```json
{
  "thread": {
    "server": "@THREAD_SERVER@",
    "board": "@THREAD_BOARD@",
    "threadkey": "@THREAD_KEY@"
  }
}
```

CMake で `@VAR@` 形式の変数を置換：

```cmake
configure_file(
    ${CMAKE_SOURCE_DIR}/etc/qNewsFlash.json.in
    ${CMAKE_BINARY_DIR}/qNewsFlash.json
    @ONLY
)
```

---

## 禁止事項

### 機密情報のコミット
```json
// 禁止: API キーを直接記述
{
  "apikey": "sk-xxxxx"  // NG
}

// OK: プレースホルダーを使用
{
  "apikey": "@API_KEY@"  // OK
}
```

### 不正な JSON 構文
```json
// 禁止: 末尾カンマ
{
  "key": "value",  // NG
}

// OK
{
  "key": "value"
}
```

### エスケープ漏れ
```json
// 禁止: エスケープされていない引用符
{
  "template": "Say "Hello""  // NG
}

// OK
{
  "template": "Say \"Hello\""  // OK
}
```

---

## XPath のベストプラクティス

### 堅牢なXPath
```json
// OK: クラスベース、構造変更に強い
"titlexpath": "//h1[contains(@class, 'title')]/text()"

// NG: 脆弱、構造変更で即座に壊れる
"titlexpath": "/html/body/div[1]/div[2]/h1/text()"
```

### 複数条件
```json
// OK: 複数条件で絞り込み
"flashxpath": "//li[contains(@class, 'news') and @data-type='flash']"
```

---

## バリデーション

### 必須フィールドチェック
```json
{
  "jijiflash": {
    "url": "...",           // 必須
    "flashxpath": "...",    // 必須
    "titlexpath": "...",    // 必須
    "urlxpath": "...",      // 必須
    "datexpath": "...",     // オプション
    "paraxpath": "..."      // オプション
  }
}
```

### 値の範囲
```json
{
  "thread": {
    "writemode": 1,  // 1, 2, 3 のいずれか
    "maxarticles": 100  // 正の整数
  }
}
```

---

## エンコーディング

- **ファイルエンコーディング**: UTF-8
- **BOM**: なし
- **改行コード**: LF (Unix)

---

## コメント

JSON はコメントをサポートしていないため、
設定の説明は別途ドキュメントに記載するか、
構造を明確にすることで対応。

```json
{
  "jijiflash": {
    "_comment_url": "時事ドットコム速報ページ",
    "url": "https://www.jiji.com/news/flash/"
  }
}
```
