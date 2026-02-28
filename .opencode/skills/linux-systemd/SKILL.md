# Linux Systemd Skill

qNewsFlash プロジェクトのための Linux Systemd/Cron スキル。
デプロイ、サービス管理、自動実行設定のベストプラクティスを提供。

---

## 概要

このスキルは、qNewsFlash を Linux システム上で
自動実行するための設定をカバーします。

**対応 OS**: RHEL/CentOS/Rocky, openSUSE/SLES, Debian/Ubuntu, Raspberry Pi OS

---

## Systemd サービス

### 基本サービスファイル (qnewsflash.service)

```ini
[Unit]
Description=qNewsFlash News Poster
Documentation=https://github.com/presire/qNewsFlash
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/qNewsFlash --sysconf=/etc/qNewsFlash/qNewsFlash.json
Restart=on-failure
RestartSec=30

# ログ
StandardOutput=journal
StandardError=journal

# リソース制限
MemoryMax=256M
CPUQuota=50%

[Install]
WantedBy=default.target
```

### タイマーファイル (qnewsflash.timer)

```ini
[Unit]
Description=qNewsFlash Timer

[Timer]
# 5分間隔で実行
OnCalendar=*:0/5
# 起動後30秒で初回実行
OnBootSec=30
# ランダム遅延 (同時実行回避)
RandomizedDelaySec=60

[Install]
WantedBy=timers.target
```

---

## リソース制限設定

### メモリ制限

```ini
[Service]
# メモリ使用量の上限
MemoryMax=256M           # 絶対値 (256MB)
MemoryMax=50%            # 割合 (システムメモリの50%)
MemoryHigh=200M          # 高しきい値 (超過すると圧縮)
MemoryLow=100M           # 低しきい値 (優先的に確保)

# スワップ使用制限
MemorySwapMax=128M       # スワップの上限

# OOM Killer 設定
OOMScoreAdjust=100       # -1000(殺されにくい)〜1000(殺されやすい)
```

### CPU 制限

```ini
[Service]
# CPU 時間の割り当て
CPUQuota=50%             # 1コアの50%使用
CPUQuota=200%            # 2コア完全使用

# CPU 優先度
Nice=10                  # 低優先度 (0: 標準, 19: 最低)
IOSchedulingClass=idle   # I/O優先度 (realtime/best-effort/idle)
```

### ファイル記述子制限

```ini
[Service]
# ファイル記述子の上限
LimitNOFILE=65536        # 同時オープンファイル数
LimitNPROC=4096          # 同時プロセス数
LimitCORE=0              # コアダンプ無効
```

### プロセス制限

```ini
[Service]
# タスク数制限
TasksMax=512             # 最大タスク数

# 実行時間制限
TimeoutStartSec=90       # 起動タイムアウト
TimeoutStopSec=30        # 停止タイムアウト
RuntimeMaxSec=3600       # 最大実行時間 (1時間後強制終了)
```

### 完全なリソース制限例

```ini
[Unit]
Description=qNewsFlash News Poster (Resource Limited)
After=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/qNewsFlash --sysconf=/etc/qNewsFlash/qNewsFlash.json
Restart=on-failure
RestartSec=30

# メモリ制限
MemoryMax=256M
MemoryHigh=200M
OOMScoreAdjust=100

# CPU制限
CPUQuota=25%
Nice=10
IOSchedulingClass=idle

# ファイル記述子
LimitNOFILE=8192
LimitNPROC=512

# プロセス制限
TasksMax=256
TimeoutStartSec=60
TimeoutStopSec=15

StandardOutput=journal
StandardError=journal

[Install]
WantedBy=default.target
```

---

## セキュリティ設定

### Sandbox 設定

```ini
[Service]
# ファイルシステム隔離
ProtectSystem=strict     # /usr, /boot, /efi を読み取り専用
ProtectHome=true         # /home, /root を無効化
ProtectKernelTunables=true  # /proc/sys, /sys 保護
ProtectControlGroups=true   # /sys/fs/cgroup 保護

# アクセス許可
ReadWritePaths=/var/log/qnewsflash /var/run/qnewsflash
ReadOnlyPaths=/etc/qNewsFlash

# 権限制限
NoNewPrivileges=true     # 新しい権限の取得禁止
CapabilityBoundingSet=   # ケーパビリティ全削除

# ネットワーク
PrivateNetwork=false     # ネットワーク使用 (true で無効化)
RestrictAddressFamilies=AF_INET AF_INET6

# ユーザー隔離
PrivateUsers=true        # ユーザー名前空間隔離
```

### ケーパビリティ制限

```ini
[Service]
# 必要最小限のケーパビリティのみ許可
CapabilityBoundingSet=CAP_NET_BIND_SERVICE CAP_NET_RAW

# または全て拒否
CapabilityBoundingSet=
```

### システムコール制限

```ini
[Service]
# 許可するシステムコールを限定
SystemCallFilter=@system-service
SystemCallFilter=~@privileged @resources

# アーキテクチャ制限
SystemCallArchitectures=native
```

### 完全なセキュリティ例

```ini
[Unit]
Description=qNewsFlash News Poster (Secure)
After=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/qNewsFlash --sysconf=/etc/qNewsFlash/qNewsFlash.json
Restart=on-failure
RestartSec=30

# 実行ユーザー (root 以外を推奨)
User=qnewsflash
Group=qnewsflash

# Sandbox
ProtectSystem=strict
ProtectHome=true
ProtectKernelTunables=true
ProtectControlGroups=true
ReadWritePaths=/var/log/qnewsflash
ReadOnlyPaths=/etc/qNewsFlash

# 権限
NoNewPrivileges=true
CapabilityBoundingSet=
PrivateUsers=true

# ネットワーク
PrivateNetwork=false
RestrictAddressFamilies=AF_INET AF_INET6

# システムコール
SystemCallFilter=@system-service
SystemCallArchitectures=native

StandardOutput=journal
StandardError=journal

[Install]
WantedBy=default.target
```

---

## 失敗時の自動復旧

### Restart ポリシー

```ini
[Service]
# 再起動ポリシー
Restart=on-failure       # 失敗時のみ再起動
# Restart=always         # 常に再起動
# Restart=on-abort       # シグナルでの異常終了時のみ
# Restart=no             # 再起動しない

# 再起動間隔
RestartSec=30            # 30秒待機
# 指数バックオフ
RestartSec=5             # 初回5秒
RestartSec=10            # 2回目10秒 (自動増加)
RestartSteps=5           # 最大増加回数

# 再起動制限
StartLimitIntervalSec=300  # 5分間に
StartLimitBurst=5          # 5回まで再起動
```

### OnFailure 連携

```ini
[Unit]
Description=qNewsFlash News Poster
OnFailure=notify-failure.service  # 失敗時に実行するサービス
OnFailureJobMode=replace          # 実行モード

[Service]
Type=simple
ExecStart=/usr/local/bin/qNewsFlash --sysconf=/etc/qNewsFlash/qNewsFlash.json
Restart=on-failure
```

### 失敗通知サービス

```ini
# /etc/systemd/system/notify-failure.service
[Unit]
Description=Notify Service Failure

[Service]
Type=oneshot
ExecStart=/usr/local/bin/notify-failure.sh %i

# notify-failure.sh の例
# #!/bin/bash
# SERVICE=$1
# logger -t systemd-failure "Service $SERVICE failed"
# # メール送信や Slack 通知などを追加
```

### ヘルスチェック

```ini
[Service]
# 定期的なヘルスチェック
WatchdogSec=60           # 60秒ごとに sd_notify("WATCHDOG=1") 必要

# ソケットアクティベーション (応答がない場合再起動)
SuccessExitStatus=SIGHUP SIGTERM
```

---

## 複数環境対応

### RHEL / CentOS / Rocky / AlmaLinux

```bash
# 必要なパッケージ
sudo dnf install systemd

# SELinux 対応
# 方法1: バイナリに適切なコンテキスト付与
sudo semanage fcontext -a -t bin_t "/usr/local/bin/qNewsFlash"
sudo restorecon -v /usr/local/bin/qNewsFlash

# 方法2: SELinux ポリシーモジュール作成
cat > qnewsflash.te << 'EOF'
module qnewsflash 1.0;

require {
    type bin_t;
    type qnewsflash_exec_t;
    class file { execute execute_no_trans };
}

type qnewsflash_exec_t;
files_type(qnewsflash_exec_t)
EOF

checkmodule -M -m -o qnewsflash.mod qnewsflash.te
semodule_package -o qnewsflash.pp -m qnewsflash.mod
sudo semodule -i qnewsflash.pp

# 設定ファイルディレクトリ
sudo mkdir -p /etc/qNewsFlash
sudo restorecon -R /etc/qNewsFlash

# ファイアウォール (外部通信が必要な場合)
sudo firewall-cmd --permanent --add-service=http
sudo firewall-cmd --permanent --add-service=https
sudo firewall-cmd --reload
```

### openSUSE / SLES

```bash
# 必要なパッケージ
sudo zypper install systemd

# AppArmor 対応
# プロファイル作成が必要な場合
sudo systemctl enable apparmor
sudo systemctl start apparmor

# 設定ファイルディレクトリ
sudo mkdir -p /etc/qNewsFlash

# YaST でのサービス管理
sudo yast2 services-manager
```

### Debian / Ubuntu

```bash
# 必要なパッケージ
sudo apt install systemd

# systemd --user が有効か確認
loginctl user-status

# ユーザーサービス用の lingering 有効化
# (ログインしていない状態でも実行)
loginctl enable-linger $USER

# 設定ファイルディレクトリ
sudo mkdir -p /etc/qNewsFlash
```

### Raspberry Pi OS

```bash
# systemd はデフォルトで有効

# メモリ制約を考慮した設定
# /etc/systemd/system/qnewsflash.service
[Service]
MemoryMax=128M
CPUQuota=25%

# SD カード書き込みを減らす設定
# /etc/systemd/journald.conf
[Journal]
Storage=volatile         # RAM にログ保存
RuntimeMaxUse=16M        # 最大16MB
```

### 環境判別スクリプト

```bash
#!/bin/bash
# install.sh - 環境に応じたインストール

detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$ID"
    elif [ -f /etc/redhat-release ]; then
        echo "rhel"
    elif [ -f /etc/debian_version ]; then
        echo "debian"
    else
        echo "unknown"
    fi
}

install_service() {
    local os=$(detect_os)
    
    case $os in
        rhel|centos|rocky|almalinux|fedora)
            sudo cp qnewsflash.{service,timer} /etc/systemd/system/
            sudo restorecon -v /etc/systemd/system/qnewsflash.*
            ;;
        opensuse|opensuse-leap|opensuse-tumbleweed|sles)
            sudo cp qnewsflash.{service,timer} /etc/systemd/system/
            ;;
        debian|ubuntu|raspbian)
            sudo cp qnewsflash.{service,timer} /etc/systemd/system/
            ;;
        *)
            echo "Unsupported OS: $os"
            exit 1
            ;;
    esac
    
    sudo systemctl daemon-reload
    sudo systemctl enable qnewsflash.timer
    sudo systemctl start qnewsflash.timer
}

install_service
```

---

## インストール手順

### ユーザーサービス

```bash
# ディレクトリ作成
mkdir -p ~/.config/systemd/user

# ファイル配置
cp qnewsflash.service ~/.config/systemd/user/
cp qnewsflash.timer ~/.config/systemd/user/

# デーモン再読み込み
systemctl --user daemon-reload

# タイマー有効化
systemctl --user enable qnewsflash.timer

# 開始
systemctl --user start qnewsflash.timer

# 状態確認
systemctl --user status qnewsflash.timer
systemctl --user list-timers

# ログ確認
journalctl --user -u qnewsflash.service -f
```

### システムサービス

```bash
# ファイル配置
sudo cp qnewsflash.service /etc/systemd/system/
sudo cp qnewsflash.timer /etc/systemd/system/

# デーモン再読み込み
sudo systemctl daemon-reload

# 有効化
sudo systemctl enable qnewsflash.timer

# 開始
sudo systemctl start qnewsflash.timer

# 状態確認
sudo systemctl status qnewsflash.timer
sudo systemctl list-timers --all

# ログ確認
sudo journalctl -u qnewsflash.service -f
```

---

## ログ管理

### journalctl での確認

```bash
# ユーザーサービスのログ
journalctl --user -u qnewsflash.service

# リアルタイム表示
journalctl --user -u qnewsflash.service -f

# 直近の100行
journalctl --user -u qnewsflash.service -n 100

# 今日のログ
journalctl --user -u qnewsflash.service --since today

# 指定期間
journalctl --user -u qnewsflash.service --since "2024-01-01" --until "2024-01-31"

# エラーのみ
journalctl --user -u qnewsflash.service -p err

# 優先度指定
journalctl --user -u qnewsflash.service -p warning  # warning以上

# JSON 形式
journalctl --user -u qnewsflash.service -o json

# カーネルログも含める
journalctl --user -u qnewsflash.service -k
```

### journald 設定

```ini
# /etc/systemd/journald.conf
[Journal]
# ストレージ設定
Storage=auto            # auto/persistent/volatile/none

# サイズ制限
SystemMaxUse=500M       # 最大ディスク使用量
SystemMaxFileSize=100M  # 最大ファイルサイズ
RuntimeMaxUse=100M      # RAM 使用量 (/run 使用時)

# 保持期間
MaxRetentionSec=7day    # 7日間保持

# 圧縮
Compress=yes            # ログ圧縮

# 転送
ForwardToSyslog=no      # syslog へ転送
ForwardToConsole=no     # コンソールへ転送
```

### ログローテーション (ファイル出力時)

```
# /etc/logrotate.d/qnewsflash
/var/log/qnewsflash.log {
    daily
    rotate 7
    compress
    delaycompress
    missingok
    notifempty
    create 0640 qnewsflash qnewsflash
    postrotate
        systemctl --user reload qnewsflash.service > /dev/null 2>&1 || true
    endscript
}
```

---

## Cron 設定 (代替方法)

### crontab 設定

```bash
# 編集
crontab -e

# 5分間隔で実行
*/5 * * * * /usr/local/bin/qNewsFlash --sysconf=/etc/qNewsFlash/qNewsFlash.json >> /var/log/qnewsflash.log 2>&1

# 毎時0分に実行
0 * * * * /usr/local/bin/qNewsFlash --sysconf=/etc/qNewsFlash/qNewsFlash.json

# 毎日朝6時と夜10時に実行
0 6,22 * * * /usr/local/bin/qNewsFlash --sysconf=/etc/qNewsFlash/qNewsFlash.json

# 平日のみ 9-17時の間1時間ごと
0 9-17 * * 1-5 /usr/local/bin/qNewsFlash --sysconf=/etc/qNewsFlash/qNewsFlash.json
```

### Cron vs Systemd Timer

| 項目 | Cron | Systemd Timer |
|------|------|---------------|
| 柔軟なスケジュール | ○ | ◎ |
| 依存関係管理 | × | ◎ |
| エラーハンドリング | × | ◎ |
| ログ統合 | × | ◎ |
| リソース制限 | × | ◎ |
| 最終実行時刻の記憶 | × | ◎ |
| ランダム遅延 | × | ◎ |
| シンプルさ | ◎ | ○ |

---

## CMake インストール設定

### CMakeLists.txt

```cmake
# Systemd オプション
set(SYSTEMD "OFF" CACHE STRING "Install systemd: OFF, USER, SYSTEM")
set_property(CACHE SYSTEMD PROPERTY STRINGS OFF USER SYSTEM)

# PID ファイルディレクトリ
set(PID "/var/run" CACHE STRING "PID file directory")

# 設定ファイルのパス置換
configure_file(
    ${CMAKE_SOURCE_DIR}/etc/qnewsflash.service.in
    ${CMAKE_BINARY_DIR}/qnewsflash.service
    @ONLY
)

configure_file(
    ${CMAKE_SOURCE_DIR}/etc/qnewsflash.timer.in
    ${CMAKE_BINARY_DIR}/qnewsflash.timer
    @ONLY
)

# ユーザーサービスインストール
if(SYSTEMD STREQUAL "USER")
    install(FILES 
        ${CMAKE_BINARY_DIR}/qnewsflash.service
        ${CMAKE_BINARY_DIR}/qnewsflash.timer
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user
    )
    message(STATUS "Installing user systemd files")
endif()

# システムサービスインストール
if(SYSTEMD STREQUAL "SYSTEM")
    install(FILES 
        ${CMAKE_BINARY_DIR}/qnewsflash.service
        ${CMAKE_BINARY_DIR}/qnewsflash.timer
        DESTINATION /etc/systemd/system
    )
    message(STATUS "Installing system systemd files")
endif()
```

### サービステンプレート (qnewsflash.service.in)

```ini
[Unit]
Description=qNewsFlash News Poster
Documentation=https://github.com/presire/qNewsFlash
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=@CMAKE_INSTALL_PREFIX@/bin/qNewsFlash --sysconf=@SYSCONF_DIR@/qNewsFlash.json
Restart=on-failure
RestartSec=30
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=default.target
```

---

## トラブルシューティング

### サービスが起動しない

```bash
# 詳細な状態確認
systemctl --user status qnewsflash.service -l

# 依存関係確認
systemctl --user list-dependencies qnewsflash.service

# 手動実行でテスト
/usr/local/bin/qNewsFlash --sysconf=/etc/qNewsFlash/qNewsFlash.json

# 権限確認
ls -la /usr/local/bin/qNewsFlash
ls -la /etc/qNewsFlash/qNewsFlash.json

# SELinux エラー確認 (RHEL系)
ausearch -m avc -ts recent | audit2why
```

### タイマーが動作しない

```bash
# タイマー一覧
systemctl --user list-timers --all

# 次回実行時刻確認
systemctl --user show qnewsflash.timer --property=NextElapseUSecRealtime

# 即時実行
systemctl --user start qnewsflash.service

# タイマーの詳細
systemctl --user show qnewsflash.timer
```

### ログが出力されない

```bash
# journald 状態確認
journalctl --verify

# ユーザージャーナル確認
ls -la ~/.local/share/journal/

# journald 設定確認
systemctl --user status systemd-journald

# ログのサイズ確認
journalctl --user --disk-usage
```

### メモリ不足で強制終了

```bash
# OOM ログ確認
journalctl --user -u qnewsflash.service | grep -i "out of memory"
dmesg | grep -i "killed process"

# メモリ使用量監視
systemctl --user show qnewsflash.service --property=MemoryCurrent
systemd-run --user --scope cat /proc/self/status | grep -i mem

# 制限を緩和
# サービスファイルで MemoryMax を増やす
```

### ネットワークエラー

```bash
# ネットワーク到達性確認
ping -c 3 news-site.example.com

# DNS 解決確認
nslookup news-site.example.com

# プロキシ設定確認
env | grep -i proxy

# ファイアウォール確認
sudo iptables -L -n
sudo firewall-cmd --list-all  # RHEL系
```

---

## 参考リンク

- [systemd Documentation](https://www.freedesktop.org/software/systemd/man/)
- [systemd.service](https://www.freedesktop.org/software/systemd/man/systemd.service.html)
- [systemd.timer](https://www.freedesktop.org/software/systemd/man/systemd.timer.html)
- [systemd.exec](https://www.freedesktop.org/software/systemd/man/systemd.exec.html)
- [systemd.resource-control](https://www.freedesktop.org/software/systemd/man/systemd.resource-control.html)
- [journalctl](https://www.freedesktop.org/software/systemd/man/journalctl.html)
- [ArchWiki - systemd](https://wiki.archlinux.org/title/Systemd)
- [ArchWiki - systemd/Timers](https://wiki.archlinux.org/title/Systemd/Timers)
