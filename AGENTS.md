# qNewsFlash - AI Agent Configuration

> このファイルは Claude Code、OpenCode、およびその他のAIエージェントがこのプロジェクトを理解し、効率的に作業するための設定ファイルです。

## Project Overview

qNewsFlash は、複数のニュースサイトからRSSおよびXPathクエリを用いてニュース記事を取得し、0ch系掲示板に自動投稿するLinux向けコンソールアプリケーションです。

### Key Features
- RSS フィードからのニュース記事取得 (時事ドットコム、共同通信等)
- XPath クエリによる速報ニュース・記事本文の抽出
- 0ch系掲示板への自動書き込み (Shift-JIS エンコーディング対応)
- 3種類の書き込みモード (単一スレッド、常時新規スレッド、ハイブリッド)
- Systemd サービス / Cron による自動実行

### Tech Stack
- **Language**: C++17
- **Framework**: Qt 5.15+ / Qt 6.x (Core, Network)
- **Libraries**: libxml2 (XML/HTML parsing, XPath), OpenSSL 3
- **Build System**: CMake 3.16+
- **Platform**: Linux only (RHEL, SUSE, Debian, Raspberry Pi OS)

---

## Build Commands

### Standard Build (Release)
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DSYSCONF_DIR=/etc/qNewsFlash \
      ..
make -j $(nproc)
```

### Debug Build
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j $(nproc)
```

### Qt Version Selection
```bash
# Qt 5 を使用する場合
cmake -DCMAKE_BUILD_TYPE=Release ..

# Qt 6 を使用する場合 (OpenSSL 3 が必要)
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### Installation
```bash
sudo make install
```

### Build Options
| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Release` | `Release` or `Debug` |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | Installation directory |
| `SYSCONF_DIR` | `/etc/qNewsFlash` | Config file directory |
| `SYSTEMD` | `OFF` | `OFF`, `USER`, or `SYSTEM` |
| `PID` | `/var/run` | PID file directory |
| `WITH_LIBXML2` | (empty) | Custom libxml2 pkgconfig path |
| `WITH_OPENSSL3` | (empty) | Custom OpenSSL3 directory |

---

## Development Setup

### Required Dependencies

#### RHEL / CentOS / Fedora
```bash
sudo dnf install coreutils make cmake gcc gcc-c++ libxml2 libxml2-devel \
                 qt5-qtbase-devel  # Qt 5 の場合
                 # または
                 qt6-qtbase-devel openssl3 openssl3-devel  # Qt 6 の場合
```

#### openSUSE / SLES
```bash
sudo zypper install coreutils make cmake gcc gcc-c++ libxml2-devel \
                    libqt5-qtbase-common-devel libQt5Core-devel libQt5Network-devel  # Qt 5
                    # または
                    qt6-base-devel qt6-core-devel qt6-network-devel openssl-3  # Qt 6
```

#### Debian / Ubuntu
```bash
sudo apt install coreutils make cmake gcc libxml2 libxml2-dev \
                  qtbase5-dev  # Qt 5
                  # または
                  qt6-base-dev openssl libssl3 libssl-dev  # Qt 6
```

### Environment Variables (Optional)
```bash
# カスタム libxml2 を使用する場合
export PKG_CONFIG_PATH="/opt/libxml2/lib/pkgconfig:$PKG_CONFIG_PATH"

# カスタム OpenSSL 3 を使用する場合
export LD_LIBRARY_PATH="/opt/openssl3/lib64:$LD_LIBRARY_PATH"
```

---

## Project Structure

```
qNewsFlash/
├── main.cpp                 # エントリポイント
├── Runner.{h,cpp}           # メイン制御・ニュース取得統括
├── HtmlFetcher.{h,cpp}      # HTTP通信・XPath処理
├── Poster.{h,cpp}           # 掲示板投稿処理
├── WriteMode.{h,cpp}        # 書き込みモード管理
├── Article.{h,cpp}          # 記事データモデル
├── JiJiFlash.{h,cpp}        # 時事ドットコム速報取得
├── KyodoFlash.{h,cpp}       # 47NEWS速報取得
├── RandomGenerator.{h,cpp}  # 高品質乱数生成
├── NtpTimeFetcher.{h,cpp}   # NTP時刻取得 (⚠️ 未使用)
├── KeyListener.{h,cpp}      # キー入力監視
├── CommandLineParser.{h,cpp}# コマンドライン解析
├── CMakeLists.txt           # ビルド設定
├── etc/                     # 設定ファイルテンプレート
│   ├── qNewsFlash.json.in
│   ├── qnewsflash.service.in
│   └── qnewsflash.timer.in
├── docs/                    # ドキュメント
├── Scripts/                 # ユーティリティスクリプト
├── .claude/                 # Claude Code設定
├── .opencode/               # OpenCode設定
└── AGENTS.md                # このファイル
```

---

## Code Style Guidelines

### C++ Standards
- **C++17** を使用
- Qt コーディング規約に従う
- Qt Smart Pointers (`QSharedPointer`, `QScopedPointer`) を優先使用

### Naming Conventions
- **Classes**: PascalCase (`HtmlFetcher`, `WriteMode`)
- **Functions/Methods**: camelCase (`fetchHtml`, `postArticle`)
- **Variables**: camelCase (`newsArticle`, `threadKey`)
- **Constants**: UPPER_SNAKE_CASE (`MAX_RETRY_COUNT`)
- **Qt Signals/Slots**: camelCase

### Code Organization
- ヘッダファイルには宣言のみ、実装は `.cpp` に
- Qt Meta-Object System 使用時は `Q_OBJECT` マクロを忘れずに
- 日本語コメント可 (UTF-8)

### Error Handling
- Qt の戻り値チェック (`QNetworkReply::error()`)
- 例外は使用せず、エラーコードまたは `bool` 戻り値で処理
- ログ出力は `qDebug()`, `qWarning()`, `qCritical()` を使用

---

## Architecture Overview

### Component Responsibilities

| Component | Responsibility |
|-----------|----------------|
| `Runner` | 全体制御、タイマー管理、ニュースソース選択 |
| `HtmlFetcher` | HTTP/HTTPS通信、RSS解析、XPath抽出 |
| `Poster` | Cookie取得、POST送信、Shift-JIS変換 |
| `WriteMode` | 3種類の書き込みモード実装、設定ファイル更新 |
| `JiJiFlash` | 時事ドットコム速報専用取得ロジック |
| `KyodoFlash` | 47NEWS速報専用取得ロジック |
| `RandomGenerator` | TSC + CSPRNG による高品質乱数生成 |

### Data Flow
```
ニュースサイト → HtmlFetcher (RSS/XPath) → Article
                                               ↓
                                         RandomGenerator (1件選択)
                                               ↓
                                         WriteMode (モード判定)
                                               ↓
                                         Poster → 0ch掲示板
```

---

## Configuration Files

### Main Config: qNewsFlash.json
- **Location**: `/etc/qNewsFlash/qNewsFlash.json` (default)
- **Format**: JSON
- **Key Sections**:
  - `newsapi`, `jiji`, `kyodo`, etc. - ニュースソース設定
  - `jijiflash`, `kyodoflash` - 速報ニュース設定
  - `thread` - 掲示板書き込み設定
  - `threadcommand` - !chtt, !hogo, !bottom コマンド設定

### XPath Configuration
XPath 式は設定ファイルで管理され、Webサイト構造変更時に更新可能。

```json
{
  "jijiflash": {
    "flashxpath": "/html/body/div[@id='Contents']/...",
    "titlexpath": "/html/head/meta[@name='title']/@content",
    "paraxpath": "/html/head/meta[@name='description']/@content"
  }
}
```

---

## Testing

### Manual Test Run
```bash
# 設定ファイルを指定して実行
./qNewsFlash --sysconf=/path/to/qNewsFlash.json

# 終了: q または Q キー → Enter
```

### Systemd Service Test
```bash
# ユーザーサービスとして起動
systemctl --user start qnewsflash.timer
systemctl --user status qnewsflash.service

# ログ確認
journalctl --user -u qnewsflash.service -f
```

### Cron Test (One-shot Mode)
```bash
# autofetch = false に設定後
./qNewsFlash --sysconf=/path/to/qNewsFlash.json
```

---

## Common Tasks

### 1. XPath 式の更新 (Webサイト構造変更時)
1. 対象サイトのHTML構造を確認
2. 新しいXPath式を作成
3. `qNewsFlash.json` の該当する `*xpath` フィールドを更新
4. テスト実行で確認

### 2. 新しいニュースソースの追加
1. 新しいクラスを作成 (既存の `JiJiFlash.cpp/h` を参考)
2. `Runner.cpp` に統合
3. `qNewsFlash.json.in` に設定項目を追加
4. `CMakeLists.txt` にソースファイルを追加

### 3. デバッグビルドでのログ確認
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j $(nproc)
./qNewsFlash --sysconf=/path/to/qNewsFlash.json
# qDebug() 出力がコンソールに表示される
```

### 4. Shift-JIS 変換問題のデバッグ
- `Poster.cpp` の `convertToShiftJIS()` 関数を確認
- 変換不可能な文字は `&#xHHHH;` 形式の文字参照に変換される

---

## Subagents

このプロジェクトでは以下の専門サブエージェントが利用可能です:

| Subagent | Specialty | Use Case |
|----------|-----------|----------|
| `qt-cpp-developer` | Qt/C++ development | Qt Network, Signal/Slot, memory management |
| `xpath-specialist` | XPath/Web scraping | News site structure changes, XPath updates |
| `cmake-builder` | CMake/Build | Dependency resolution, cross-compilation |
| `research-collector` | Technical research | API documentation, library research |

---

## Important Notes

### Security
- API keys are stored in `qNewsFlash.json` - **never commit to git**
- HTTPS is used for all external communications
- OpenSSL 3 is required for Qt 6 builds

### Encoding
- All source files: **UTF-8**
- POST data to bulletin boards: **Shift-JIS**
- Config file: **UTF-8**

### Platform
- **Linux only** - Windows/macOS is not supported
- x86_64 and ARM, RISC-V64 are supported

---

## References

- **GitHub**: https://github.com/presire/qNewsFlash
- **Qt Documentation**: https://doc.qt.io/
- **libxml2**: https://gitlab.gnome.org/GNOME/libxml2
- **Project Docs**: `docs/` directory (要件定義書, システム概要, etc.)
