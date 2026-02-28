# qNewsFlash - Claude Code Configuration

> このファイルは常にロードされるマスター設定です。
> プロジェクト概要、ビルド手順、コーディング規約を含みます。

## Project Overview

qNewsFlash は、複数のニュースサイトからRSSおよびXPathクエリを用いてニュース記事を取得し、0ch系掲示板に自動投稿するLinux向けコンソールアプリケーションです。

### Tech Stack
- **Language**: C++17
- **Framework**: Qt 5.15+ / Qt 6.x (Core, Network)
- **Libraries**: libxml2 (XML/HTML parsing, XPath), OpenSSL 3
- **Build System**: CMake 3.16+
- **Platform**: Linux only (RHEL, SUSE, Debian, Raspberry Pi OS)

---

## Build Commands

### Standard Build
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

### Install
```bash
sudo make install
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
├── docs/                    # ドキュメント
├── Scripts/                 # ユーティリティスクリプト
└── .claude/                 # Claude Code設定
```

---

## Code Style

### Naming Conventions
- **Classes**: PascalCase (`HtmlFetcher`, `WriteMode`)
- **Functions/Methods**: camelCase (`fetchHtml`, `postArticle`)
- **Variables**: camelCase (`newsArticle`, `threadKey`)
- **Constants**: UPPER_SNAKE_CASE (`MAX_RETRY_COUNT`)

### C++ Standards
- **C++17** を使用
- Qt コーディング規約に従う
- Qt Smart Pointers (`QSharedPointer`, `QScopedPointer`) を優先使用
- 例外は使用せず、エラーコードまたは `bool` 戻り値で処理
- ログ出力は `qDebug()`, `qWarning()`, `qCritical()` を使用

### Error Handling
- Qt の戻り値チェック (`QNetworkReply::error()`)
- 空の catch ブロックは禁止

---

## Configuration

### Main Config: qNewsFlash.json
- **Location**: `/etc/qNewsFlash/qNewsFlash.json`
- **Format**: JSON
- **Key Sections**:
  - `newsapi`, `jiji`, `kyodo` - ニュースソース設定
  - `jijiflash`, `kyodoflash` - 速報ニュース設定
  - `thread` - 掲示板書き込み設定

### XPath Configuration
XPath 式は設定ファイルで管理され、Webサイト構造変更時に更新可能。

---

## Important Notes

### Security
- API keys are stored in `qNewsFlash.json` - **never commit to git**
- HTTPS is used for all external communications

### Encoding
- All source files: **UTF-8**
- POST data to bulletin boards: **Shift-JIS**
- Config file: **UTF-8**

### Platform
- **Linux only** - Windows/macOS is not supported

---

## Available Resources

### Commands
- `/build` - Build the project
- `/test` - Run tests
- `/lint` - Check code style

### Skills
- `qt-cpp-development` - Qt5/Qt6 C++ development
- `xpath-query` - XPath/Web scraping
- `linux-systemd` - Systemd/Cron deployment

### Subagents
- `qt-cpp-developer` - Qt/C++ specialist
- `xpath-specialist` - XPath specialist
- `cmake-builder` - CMake/Build specialist
- `research-collector` - Technical research

### Rules
- `cpp-code.rules.md` - C++/Qt coding rules
- `cmake.rules.md` - CMake rules
- `json-config.rules.md` - JSON config rules
