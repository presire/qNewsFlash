# qNewsFlash
<br>

URL : https://github.com/presire/qNewsFlash  
<br>

# はじめに  
qNewsFlashは、News APIや時事ドットコム等のニュース記事を取得して、JSONファイルに出力するソフトウェアです。  
現在対応しているニュースサイトは、以下の通りです。  

* News API (無料版のNews APIはニュース記事が1日遅れのため、デフォルトでは無効)  
* 時事ドットコム  
* 共同通信  
* 朝日新聞デジタル (ただし、有料記事が多いです)  
* CNET Japan  
* ハンギョレ新聞  

<br>

**このソフトウェアは、Qt 5.15 および libxml 2.0を使用して開発されているため、動作させるためにはそれらのライブラリが必要となります。**  
<br>

この記事では、Red Hat Enterprise LinuxおよびSUSE Enterprise Linux / openSUSEを前提に記載しております。  
また、特別な操作無しで、他のLinuxディストリビューションにもインストールして使用できると思います。  
(例: Debian GNU/Linux, Fedora, Manjaro, ... など)  
<br>

**注意：**  
**version 0.1.0から試験的ですが、掲示板等に書き込めるようになりました。**  
**また、要望があれば、逐次開発を進めていく予定です。**  
<br>
<br>

# 1. ビルドに必要なライブラリをインストール  
<br>

* Qt5 Core
* Qt5 Network
  * <https://www.qt.io/>  
  * qNewsFlashは、Qtを使用しています。
  * 上記で使用しているQtライブラリは、LGPL v3オープンソースライセンスの下で利用可能です。  
  * Qtのライセンスファイルは、以下に示すファイルで確認できます。  
    <I>**LibraryLicenses/Qt/LGPL-3.0**</I>  
<br>

* libxml2.0
  * <https://gitlab.gnome.org/GNOME/libxml2>  
  * qNewsFlashは、libxml 2.0を使用しています。  
  * libxml 2.0は、MITライセンスの下で利用可能です。  
  * libxml 2.0のライセンスファイルは、以下に示すファイルで確認できます。  
    <I>**LibraryLicenses/libxml2/LICENSE.MIT**</I>  
<br>


システムをアップデートした後、続いてビルドに必要なライブラリをインストールします。  

    # RedHat Enterprise Linux
    sudo dnf update   
    sudo dnf install coreutils coreutils-common make cmake gcc gcc-c++ \  
                     libxml2 libxml2-devel qt5-qtbase-devel  

    # SLE / openSUSE
    sudo zypper update  
    sudo zypper install coreutils make cmake gcc gcc-c++ libxml2-devel \  
                        libqt5-qtbase-common-devel libQt5Core-devel \  
                        libQt5Network-devel  
<br>
<br>

# 2. ビルドおよびインストール
## 2.1. qNewsFlashのビルドおよびインストール
GitHubからqNewsFlashのソースコードをダウンロードします。  

    git clone https://github.com/presire/qNewsFlash.git qNewsFlash  

    cd qNewsFlash  

    mkdir build && cd build  
<br>

qNewsFlashをソースコードからビルドするには、<code>cmake</code>コマンドを使用します。  
Ninjaを使用する場合は、<code>cmake</code>コマンドに<code>-G Ninja</code>オプションを付加します。  
<br>

ビルド時に使用できるオプションを以下に示します。  
<br>

* <code>CMAKE_BUILD_TYPE</code>  
  デフォルト値 : <code>Release</code>  
  リリースビルドまたはデバッグビルドを指定します。  
<br>
* <code>CMAKE_INSTALL_PREFIX</code>  
  デフォルト値 : <code>/usr/local</code>  
  qNewsFlashおよびqNewsFlash.shのインストールディレクトリを変更することができます。  
<br>
* <code>SYSCONF_DIR</code>  
  デフォルト値 : <code>/etc/qNewsFlash/qNewsFlash.json</code>  
  設定ファイル qNewsFlash.jsonのインストールディレクトリを変更することができます。  
<br>
* <code>SYSTEMD</code>  
  デフォルト値 : <code>OFF</code>  (無効 : Systemdサービスファイルはインストールされない)  
  <br>
  指定できる値 1 : <code>system</code>を指定する場合 - <code>/etc/systemd/system</code>  
  指定できる値 2 : <code>user</code>を指定する場合 - <code>~/.config/systemd/user</code>  
  <br>
  Systemdサービスファイル qNewsFlash.serviceのインストールディレクトリを変更することができます。  
<br>
* <code>PID</code>  
  デフォルト値 : <code>/var/run</code>  
  Systemdサービスを使用する場合、プロセスファイル qNewsFlash.pidのパスを変更することができます。  
  Systemdサービスを使用しない場合は不要です。  
<br>
* <code>WITH_LIBXML2</code>  
  デフォルト値 : 空欄  
  使用例 : <code>-DWITH_LIBXML2=/opt/libxml2/lib64/pkgconfig</code>  
  ( libxml 2.0ライブラリが、/opt/libxml2ディレクトリにインストールされている場合 )  
  <br>
  libxml 2.0ライブラリのpkgconfigディレクトリのパスを指定することにより、  
  任意のディレクトリにインストールされているlibxml 2.0ライブラリを使用して、このソフトウェアをコンパイルすることができます。  
  通常は、あまり使用しないと思われます。  

<br>

    cmake -DCMAKE_BUILD_TYPE=Release \  
          -DCMAKE_INSTALL_PREFIX=<"qNewsFlashのインストールディレクトリ"> \  
          -SYSCONF_DIR=<"設定ファイルのインストールディレクトリ">           \  
          -DSYSTEMD=user  \  # Systemdサービスファイルをインストールする場合  
          -DPID=/tmp      \  # Systemdサービスを使用する場合
          ..  
    
    make -j $(nproc)  
    
    make install  または  sudo make install  
<br>

## 2.2. Systemdサービスの使用

qNewsFlashのSystemdサービスファイルは、  
/etc/systemd/systemディレクトリ、または、~/.config/systemd/userディレクトリのいずれかにインストールされております。  
<br>

<code>cmake</code>コマンドにおいて、<code>SYSTEMD</code>オプションを変更することによりインストールディレクトリが変更されます。  
<br>

Systemdサービスを再読み込みします。  

    sudo systemctl daemon-reload  
    または  
    systemctl --user daemon-reload  
<br>

qNewsFlashを実行する時は、qnewsflashデーモンを開始します。  

    sudo systemctl start qnewsflash.service
    または
    systemctl --user start qnewsflash.service
<br>

PCの起動時またはユーザのログイン時において、qNewsFlashデーモンを自動起動する場合は、以下に示すコマンドを実行します。  

    sudo systemctl enable qnewsflash.service  
    または  
    systemctl --user enable qnewsflash.service  
<br>

qNewsFlashを停止する場合は、以下に示すコマンドを実行します。

    sudo systemctl stop qnewsflash.service
    または
    systemctl --user stop qnewsflash.service
<br>

## 2.3. 直接実行する

Systemdサービスを使用せずに、qNewsFlashを実行することもできます。  

    sudo qNewsFlash --sysconf=<設定ファイル qNewsFlash.jsonのパス>
    または
    qNewsFlash --sysconf=<設定ファイル qNewsFlash.jsonのパス>  
<br>

直接実行した場合において、**[q]キー** または **[Q]キー** ==> **[Enter]キー** を押下することにより、qNewsFlashを終了することができます。  
<br>
<br>


# 3. ラッパーシェルスクリプト - qNewsFlash.sh

このソフトウェアには、Qt 5ライブラリが同梱されております。  
もし、Qt 5ライブラリがインストールできない環境でも、qNewsFlash.shファイルを使用することにより、ソフトウェアを動作させることができる可能性があります。  
<br>

なお、qNewsFlash.shファイルは、qNewsFlashファイルと同階層のディレクトリにインストールされております。  
デフォルトでは、<I>**/usr/local/bin**</I> ディレクトリにインストールされます。  
<br>

qNewsFlash.shファイルの内容を以下に示します。  

    #!/usr/bin/env sh

    # qNewsFlashのファイル名
    appname="qNewsFlash"

    # 絶対パスの取得
    dirname="$(dirname -- "$(readlink -f -- "${0}")" )"

    if [ "$dirname" = "." ]; then
        dirname="$PWD/$dirname"
    fi

    cd $dirname

    # Qt 5ライブラリのパスを環境変数LD_LIBRARY_PATHに追加
    export LD_LIBRARY_PATH="/<Qt 5のインストールディレクトリ>/lib64/Qt:$LD_LIBRARY_PATH"

    # qNewsFlashの実行
    "$dirname/$appname" "$@" 
<br>

同梱しているQt 5ライブラリは、デフォルトでは<I>**/usr/local/(lib | lib64)/Qt**</I> ディレクトリにインストールされます。  

必要であれば、ユーザ自身がQt 5ライブラリを任意のディレクトリに配置して、環境変数<code>LD_LIBRARY_PATH</code>の値を変更することもできます。  
<br>

qNewsFlash.shを実行します。  
qNewsFlashが正常に実行できるかどうかを確認してください。  

    sudo qNewsFlash.sh --sysconf=<設定ファイル qNewsFlash.jsonのパス>
    または
    qNewsFlash.sh --sysconf=<設定ファイル qNewsFlash.jsonのパス>  
<br>

Systemdサービスを使用する場合は、<I>**ExecStart**</I> キーの値も変更します。  

<code>
ExecStart=/<qNewsFlashのインストールディレクトリ>/bin/qNewsFlash.sh --sysconf=<設定ファイル qNewsFlash.jsonのパス>  
</code>
<br>
<br>


# 4. qNewsFlashの設定 - qNewsFlash.jsonファイル

qNewsFlashの設定ファイルであるqNewsFlash.jsonファイルでは、  
取得するニュースサイトの有無、更新時間の間隔、取得したニュース記事を書き込むためのファイル、ログファイルのパス等が設定できます。  

この設定ファイルがあるデフォルトのディレクトリは、<I>**/etc/qNewsFlash/qNewsFlash.json**</I> です。  
<code>cmake</code>コマンドの実行時にディレクトリを変更することもできます。  
<br>

各設定の説明を記載します。  
<br>

* newsapi  
  デフォルト値 : <code>false</code>  
  News APIからニュースを取得するかどうかを指定します。  
  無料版のNews APIは、ニュース記事が1日遅れのため、デフォルトは無効です。  
  なお、有料版のNews APIビジネスにおいて、月額$449となっております。  
<br>
* api  
  デフォルト値 : 空欄  
  News APIからニュースを取得する場合、APIキーが必要となります。  
  そのAPIキーを指定します。  
<br>
* exclude  
  デフォルト値 : <code>["Kbc.co.jp", "Sponichi.co.jp", "Bunshun.jp", "Famitsu.com", "Sma.co.jp",  "Oricon.co.jp", "Jleague.jp", "YouTube"]</code>  
  News APIから取得するニュース記事において、除外するメディアを指定します。  
 <br>
* jiji  
  デフォルト値 : <code>true</code>  
  時事ドットコムからニュースを取得するかどうかを指定します。  
  デフォルトは有効です。  
 <br>
* kyodo  
  デフォルト値 : <code>true</code>  
  共同通信からニュースを取得するかどうかを指定します。  
  デフォルトは有効です。  
 <br>
* asahi  
  デフォルト値 : <code>false</code>  
  朝日デジタルからニュースを取得するかどうかを指定します。  
  デフォルトは無効です。  
 <br>
* cnet  
  デフォルト値 : <code>true</code>  
  CNET Japanからニュースを取得するかどうかを指定します。  
  デフォルトは有効です。  
<br>
* hanj  
  デフォルト値 : <code>false</code>  
  ハンギョレ新聞 ジャパンからニュースを取得するかどうかを指定します。  
  デフォルトは無効です。  
<br>
* maxpara  
  デフォルト値 : <code>100</code>  
  各ニュース記事の本文の最大文字数を指定します。  
  デフォルトは最大100文字です。 
<br>
* interval  
  デフォルト値 : <code>1800</code>  
  各ニュースサイトからニュース記事を取得する時間間隔 (秒) を指定します。  
  デフォルト値は1800[秒] (30分間隔でニュース記事を取得) です。  
  <br>
  180秒未満 (3[分]未満) を指定した場合は、強制的に180[秒] (3[分]) に指定されます。  
  0未満や不正な値が指定された場合は、強制的に1800[秒] (30[分]) に指定されます。  
  <br>
  <u>より多くのニュース記事を読む込む場合、時間が掛かることが予想されます。</u>  
  <u>その場合、大きめの数値を指定したほうがよい可能性があります。</u>  
<br>
* writefile <u>**(version 0.1.0以降は無効)**</u>  
  デフォルト値 : <code>/tmp/qNewsFlashWrite.json</code>  
  このソフトウェアは、各ニュースサイトからニュース記事群からニュース記事を自動的に1つ選択します。  
  このファイルは、その選択された1つのニュース記事の情報が記載されています。  
  <br>
  このファイルを利用して、ユーザは掲示板等に自動的に書き込むようなスクリプトを作成することができます。  
<br>
* logfile  
  デフォルト値 : <code>/var/log/qNewsFlash_log.json</code>  
  上記のニュース記事が自動的に1つ選択された時、選択された各記事のログを保存しています。  
  ただし、当日に選択された記事のみを保存しているため、前日のログは自動的に削除されます。  
  <br>
  前日の記事が削除されるタイミングは、日付が変わった時の最初の更新時です。  
<br>
* update  
  デフォルト値 : 空欄  
  ニュース記事を取得した直近の時間です。  
  ニュース記事を取得した時に自動的に更新されます。  
  ユーザはこの値を書き換えないようにしてください。  
* requesturl  
  デフォルト値 : 空欄  
  ニュース記事を書き込むため、POSTデータを送信するURLを指定します。  
  <br>
  0ch系の場合は、<code>**http(s)://<ドメイン名>/test/bbs.cgi?guid=ON**</code> の場合が多いです。  
  書き込み時に必須です。  
<br>
* subject  
  デフォルト値 : 空欄  
  POSTデータとして送信します。  
<br>
* from  
  デフォルト値 : 空欄  
  POSTデータとして送信します。  
<br>
* mail  
  デフォルト値 : 空欄  
  フォームのメール欄に入力する文字列を指定します。  
  POSTデータとして送信します。  
<br>
* bbs  
  デフォルト値 : 空欄  
  掲示板のBBS名を指定します。  
  POSTデータとして送信します。  
  書き込み時に必須です。  
<br>
* time  
  デフォルト値 : 空欄  
  書き込む時間を指定します。  
  掲示板によっては、この値は意味をなさない可能性があります。  
  POSTデータとして送信します。  
  書き込み時に必須だと思われます。  
<br>
* key  
  デフォルト値 : 空欄  
  ニュース記事を書き込むスレッド番号を指定します。  
  POSTデータとして送信します。  
  書き込み時に必須です。  
<br>
* shiftjis  
  デフォルト値 : <code>true</code>  
  POSTデータの文字コードをShift-JISに変換するかどうかを指定します。  
  0ch系は、Shift-JISを指定 (<code>**true**</code>) することを推奨します。  

<br>

    {  
      "api": "",  
      "asahi": false,  
      "cnet": true,  
      "exclude": [  
        "Kbc.co.jp",  
        "Sponichi.co.jp",  
        "Bunshun.jp",  
        "Famitsu.com",  
        "Sma.co.jp",  
        "Oricon.co.jp",  
        "Jleague.jp",  
        "YouTube"  
      ],  
      "hanj": false,  
      "interval": "1800",  
      "jiji": true,  
      "kyodo": true,  
      "logfile": "/var/log/qNewsFlash_log.json",  
      "maxpara": "100",  
      "newsapi": false,  
      "update": "",  
      "writefile": "/tmp/qNewsFlashWrite.json",  
      "requesturl": "",  
      "subject": "",  
      "from": "",  
      "mail": "",  
      "bbs": "",  
      "time": "",  
      "key": "",  
      "shiftjis": true  
    }  
<br>
<br>


# 5 <del>書き込み用の記事ファイル - qNewsFlashWrite.jsonファイル</del><br>**この設定ファイルは現在使用されておりません**  


上記のセクションでも記載した通り、  
このソフトウェアは、各ニュースサイトからニュース記事を複数取得して、その複数のニュース記事から自動的に1つのみを選択します。  
このファイルは、その選択された(書き込み用)1つのニュース記事の情報が記載されています。 
<br>

記事の更新のタイミングにより、ファイル内容は上書きされます。  

ユーザは、このファイルを利用して掲示板等に自動的に書き込むようなスクリプトを作成することができます。  
<br>

また、設定ファイルにより、このファイル名およびファイルのパスを自由に変更することができます。  
<br>

このファイルは、以下に示すようなフォーマットになっています。  

    {
      "date": "2024年3月6日 8時1分",
      "paragraph": "米マイクロソフト（ＭＳ）は５日、基本ソフト（ＯＳ）「ウィンドウズ１１」搭載のパソコンで米グーグルの",
      "title": "アンドロイドアプリ対応終了　ウィンドウズ１１、２５年に",
      "url": "https://www.jiji.com/jc/article?k=2024030600270"
    }
<br>
<br>


# 6. ログファイル - qNewsFlash_log.json

上記のセクションでも記載した通り、  
ニュース記事が自動的に1つ選択された時、選択された各記事のログを保存しています。  
ただし、当日に選択された記事のみを保存しているため、前日のログは自動的に削除されます。  
<br>

前日の記事が削除されるタイミングは、日付が変わった時の最初の更新時です。  
<br>

また、設定ファイルにより、このファイル名およびパスを自由に変更することができます。  
<br>

このファイルは、以下に示すようなフォーマットになっています。 

    [
      {
        "date": "2024年3月6日 0時28分",
        "paragraph": "国際刑事裁判所（ＩＣＣ、オランダ・ハーグ）は５日、ロシア軍幹部２人に対し、ウクライ",
        "title": "ロシア軍幹部２人に逮捕状　ウクライナの民間インフラ攻撃で―国際刑事裁",
        "url": "https://www.jiji.com/jc/article?k=2024030600022"
      },
      {
        "date": "2024年3月6日 0時20分",
        "paragraph": "米ハイアット財団は５日、「建築界のノーベル賞」と呼ばれる２０２４年のプリツカー賞を",
        "title": "米プリツカー賞に山本理顕氏　建築のノーベル賞、日本人９人目",
        "url": "https://www.jiji.com/jc/article?k=2024030501253"
      },
      {
        "date": "2024年3月6日 8時1分",
        "paragraph": "米マイクロソフト（ＭＳ）は５日、基本ソフト（ＯＳ）「ウィンドウズ１１」搭載のパソコ",
        "title": "アンドロイドアプリ対応終了　ウィンドウズ１１、２５年に",
        "url": "https://www.jiji.com/jc/article?k=2024030600270"
      }
    ]
<br>
<br>