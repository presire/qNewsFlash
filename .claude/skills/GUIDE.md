# Skills Guide

スキルは、特定のタスクを実行するためのワークフローと知識を定義します。
ユーザーが明示的に呼び出すか、タスク内容に基づいて自動的に適用されます。

---

## 利用可能なスキル

| スキル | 説明 | 呼び出し例 |
|--------|------|------------|
| qt-cpp-development | Qt5/Qt6 C++開発 | `qt-cpp-developmentスキルを使用して...` |
| xpath-query | XPath/Webスクレイピング | `xpath-queryスキルでXPath式を作成` |
| linux-systemd | Systemd/Cron デプロイ | `linux-systemdスキルでサービス設定` |

---

## 使用方法

### 明示的呼び出し

```
qt-cpp-developmentスキルを使用して、QNetworkAccessManagerでHTTP GETリクエストを実装してください。
```

```
xpath-queryスキルを使用して、時事ドットコムの速報ニュースを取得するXPath式を作成してください。
```

```
linux-systemdスキルを使用して、qNewsFlashのsystemd timerを設定してください。
```

### 自動適用

タスク内容に基づいて適切なスキルが自動的に選択されます：

| トリガー語 | 適用スキル |
|------------|------------|
| Qt, QNetwork, QNetworkAccessManager, QNetworkReply | qt-cpp-development |
| Signal/Slot, connect, QObject | qt-cpp-development |
| QString, QTextCodec, QStringEncoder, エンコーディング | qt-cpp-development |
| QTimer, 定期実行, タイマー | qt-cpp-development |
| XPath, xpath式, //, @, contains() | xpath-query |
| スクレイピング, Webパース, HTML抽出 | xpath-query |
| libxml2, xmlParse, htmlReadMemory | xpath-query |
| ニュースサイト, 記事取得, RSS | xpath-query |
| systemd, service, timer, unit | linux-systemd |
| cron, crontab, 定期実行 | linux-systemd |
| journalctl, ログ, journald | linux-systemd |
| サービス登録, 自動起動, デーモン | linux-systemd |

---

## スキルの構成

各スキルは以下の構造を持ちます：

```
skills/
├── GUIDE.md               # このファイル
├── qt-cpp-development/
│   └── SKILL.md           # Qt/C++開発スキル定義
├── xpath-query/
│   └── SKILL.md           # XPath/Webスクレイピングスキル定義
└── linux-systemd/
    └── SKILL.md           # Systemd/Cron スキル定義
```

---

## スキル詳細

### qt-cpp-development

Qt5/Qt6 C++開発のためのスキル。ネットワーク通信、文字列処理、メモリ管理をカバー。

**適用場面:**
- HTTP/HTTPS通信の実装
- Shift-JIS エンコーディング変換
- JSON設定ファイル解析
- QTimer による定期実行
- Qt Smart Pointers の使用
- Signal/Slot パターンの実装
- ネットワークリトライ処理
- Qt5/Qt6 互換性対応

**主なトピック:**
| トピック | 内容 |
|----------|------|
| Qt Network | QNetworkAccessManager, QNetworkReply, SSL/TLS |
| 文字列処理 | QString, QTextCodec (Qt5), QStringEncoder (Qt6) |
| 非同期処理 | QTimer, QFuture, QtConcurrent |
| メモリ管理 | QSharedPointer, QScopedPointer, deleteLater |
| エラー処理 | ネットワークエラー, タイムアウト, リトライ |

**詳細:** `qt-cpp-development/SKILL.md`

### xpath-query

libxml2 を用いた XPath クエリと Web スクレイピングのスキル。

**適用場面:**
- ニュースサイトからの記事抽出
- XPath 式の作成・最適化
- HTML/XML パース処理
- Webサイト構造変更への対応
- RSS フィード解析

**主なトピック:**
| トピック | 内容 |
|----------|------|
| XPath 基礎 | ロケーションパス, 述語, 関数, 軸 |
| libxml2 API | htmlReadMemory, xmlXPathEvalExpression |
| エンコーディング | UTF-8, Shift-JIS, EUC-JP 対応 |
| パターン集 | ニュースサイト別の XPath パターン |
| トラブルシューティング | マッチしない, 文字化け, メモリリーク |

**詳細:** `xpath-query/SKILL.md`

### linux-systemd

Linux システムでのデプロイ、サービス管理、自動実行設定のスキル。

**適用場面:**
- Systemd サービス・タイマー設定
- Cron ジョブ設定
- ログ管理 (journalctl)
- CMake インストール設定
- リソース制限・セキュリティ設定

**主なトピック:**
| トピック | 内容 |
|----------|------|
| サービス設定 | qnewsflash.service, Restart, OnFailure |
| タイマー設定 | qnewsflash.timer, OnCalendar |
| リソース制限 | MemoryMax, CPUQuota, LimitNOFILE |
| セキュリティ | ProtectSystem, NoNewPrivileges |
| OS別設定 | RHEL, openSUSE, Debian, Raspberry Pi |

**詳細:** `linux-systemd/SKILL.md`

---

## スキル組み合わせの使用例

### ニュースソース追加

```
xpath-queryスキルとqt-cpp-developmentスキルを組み合わせて、
新しいニュースサイトを追加する実装を行ってください。
```

**実行フロー:**
1. xpath-query: サイト構造分析、XPath式作成
2. qt-cpp-development: 新しいクラス実装、HTTP通信

### 定期実行システム構築

```
qt-cpp-developmentスキルとlinux-systemdスキルを組み合わせて、
ニュース取得の定期実行システムを構築してください。
```

**実行フロー:**
1. qt-cpp-development: QTimer実装、定期実行ロジック
2. linux-systemd: Systemd timer設定、ログ管理

### Webサイト構造変更対応

```
xpath-queryスキルとlinux-systemdスキルを組み合わせて、
Webサイトの構造変更に対応した設定更新とデプロイを行ってください。
```

**実行フロー:**
1. xpath-query: 新しいHTML構造分析、XPath更新
2. linux-systemd: サービス再起動、ログ確認

### エンコーディング問題解決

```
qt-cpp-developmentスキルとxpath-queryスキルを組み合わせて、
Shift-JISエンコーディングのニュースサイト対応を修正してください。
```

**実行フロー:**
1. xpath-query: HTMLのエンコーディング確認
2. qt-cpp-development: QStringEncoder/QTextCodec実装

---

## スキルとサブエージェントの違い

| 特徴 | スキル | サブエージェント |
|------|--------|------------------|
| コンテキスト | 共有 | 独立 |
| 実行形態 | ワークフロー | 自律的エージェント |
| 呼び出し | 明示的/自動 | タスク委譲 |
| 用途 | 手順・知識の提供 | 複雑なタスクの委譲 |
| 応答速度 | 即座 | 処理時間あり |
| 対話 | 不可 | 可能 |

**使い分け:**
- パターンやベストプラクティスを知りたい → スキル
- 複雑な実装を任せたい → サブエージェント
- 即座に情報が必要 → スキル
- 深い調査や分析が必要 → サブエージェント

---

## よくある質問

### Q: スキルはいつ自動適用されますか？

タスクの内容にトリガー語が含まれている場合、自動的に適用されます。
明示的に指定する場合は、「〜スキルを使用して」と記述してください。

### Q: 複数のスキルを同時に使用できますか？

はい。タスクの性質に応じて複数のスキルが自動的に組み合わされます。
例えば、ネットワーク通信とXPath処理を含むタスクでは、qt-cpp-development
とxpath-queryの両方が適用されます。

### Q: スキルの内容を確認できますか？

各スキルディレクトリ内の SKILL.md ファイルを参照してください。
詳細な実装パターン、コード例、トラブルシューティングが含まれています。

### Q: スキルとサブエージェントどちらを使うべきですか？

- **スキル**: パターンを学びたい、コード例が見たい、手順を知りたい
- **サブエージェント**: 実装を任せたい、調査してほしい、複雑な問題を解決したい

---

## 参考リンク

- [Qt Documentation](https://doc.qt.io/)
- [libxml2 Documentation](http://xmlsoft.org/)
- [systemd Documentation](https://www.freedesktop.org/software/systemd/man/)
- [qNewsFlash GitHub](https://github.com/presire/qNewsFlash)
