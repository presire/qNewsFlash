cmake_minimum_required(VERSION 3.15)
project(qNewsFlash VERSION 0.1.5 LANGUAGES CXX)


# Linuxのみビルドを許可
if(NOT UNIX OR APPLE)
  message(FATAL_ERROR "本ソフトウェアは、Linuxでのみビルドできます")
endif()


set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# ビルドタイプ
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")

## ビルドタイプを表示
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("")
    message("qNewsFlash : Debug Build.")
    message("")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message("")
    message("qNewsFlash : Release Build.")
    message("")
endif()


# デフォルトのインストールディレクトリ
set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Installation directory")


# pkg-configの使用
find_package(PkgConfig REQUIRED)


# WITH_LIBXML2オプションに"libxml-2.0.pc"ファイルのパスを指定する場合、任意のディレクトリに存在するlibxml2.0ライブラリを使用する
set(WITH_LIBXML2 "" CACHE STRING "Path to libxml-2.0.pc file")

# WITH_LIBXML2が空ではない場合、その値を環境変数PKG_CONFIG_PATHに追加
if(NOT WITH_LIBXML2 STREQUAL "")
    set(ENV{PKG_CONFIG_PATH} "${WITH_LIBXML2}:$ENV{PKG_CONFIG_PATH}")
endif()


# pkg-configを使用してlibxml2ライブラリを検索
pkg_search_module(LIBXML2 REQUIRED libxml-2.0)


# ライブラリのインクルードディレクトリをターゲットに追加
include_directories(${LIBXML2_INCLUDE_DIRS})


# ライブラリのリンクディレクトリをターゲットに追加
link_directories(${LIBXML2_LIBRARY_DIRS})


# pkg-configを使用してQtライブラリを検索
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)

## Qtのpkg-configファイル
pkg_check_modules(QT_CORE     Qt${QT_VERSION_MAJOR}Core REQUIRED IMPORTED_TARGET)
pkg_check_modules(QT_NETWORK  Qt${QT_VERSION_MAJOR}Network REQUIRED IMPORTED_TARGET)

## Qt 5.15未満はビルドエラー
#set(QT_MINIMUM_VERSION 5.15)
if(QT_VERSION VERSION_LESS 5.15.0)
    message(FATAL_ERROR "Qt version must be greater than or equal to 5.15.0")
endif()


add_executable(qNewsFlash
        main.cpp
        Runner.h Runner.cpp
        HtmlFetcher.h HtmlFetcher.cpp
        Article.h Article.cpp
        RandomGenerator.h RandomGenerator.cpp
        Poster.h Poster.cpp
)


# プリプロセッサの定義
## デバッグビルドの場合、_DEBUGプリプロセッサを定義
target_compile_definitions(qNewsFlash PRIVATE
        $<$<CONFIG:Debug>:_DEBUG>
)

## バージョンが0.1.0未満の場合、_BELOW_0_1_0プリプロセッサを定義
if(PROJECT_VERSION VERSION_LESS 0.1.0)
    target_compile_definitions(qNewsFlash PRIVATE
        _BELOW_0_1_0
    )
endif()

## ARMまたはAARCH64かどうかを確認
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    # NEONをサポートする場合 (主に64ビットARM)
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-mfpu=neon" COMPILER_SUPPORTS_NEON)
    if(COMPILER_SUPPORTS_NEON)
        target_compile_definitions(qNewsFlash PRIVATE ARM_NEON)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=neon")
    else()
        # WMMXをサポートする場合 (主に32ビットARM)
        # WMMXのサポートは、特定のコンパイラやプラットフォームに依存する可能性があるため、
        # 実際にコンパイラがWMMXをサポートしているかどうかを確認する必要がある
        CHECK_CXX_COMPILER_FLAG("-march=armv7-a+simd" COMPILER_SUPPORTS_WMMX)
        if(COMPILER_SUPPORTS_WMMX)
            target_compile_definitions(qNewsFlash PRIVATE ARM_WMMX)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a+simd")
        endif()
    endif()
endif()

## バージョン情報
target_compile_definitions(qNewsFlash PRIVATE
        PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)


# libxml2のヘッダファイル
target_include_directories(qNewsFlash PRIVATE
        ${LIBXML2_INCLUDE_DIRS}
)


target_link_libraries(qNewsFlash
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        ${LIBXML2_LIBRARIES}
)


# libxml2のコンパイルオプション
add_definitions(
        ${LIBXML2_CFLAGS_OTHER}
)


include(GNUInstallDirs)


# 設定ファイルのオプション
set(SYSCONF_DIR "/etc/qNewsFlash" CACHE PATH "Directory to install qNewsFlash.json")

## qNewsFlash.json.inからqNewsFlash.jsonを生成
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/etc/qNewsFlash.json.in ${CMAKE_CURRENT_BINARY_DIR}/etc/qNewsFlash.json @ONLY)

## 設定ファイルのインストールルール
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/etc/qNewsFlash.json
        DESTINATION ${SYSCONF_DIR}
)


## PID (プロセスID) ディレクトリのオプション
set(PID "/var/run" CACHE PATH "Directory to qNewsFlash.pid")


# Systemdサービスファイルのインストールルール
## SYSTEMDオプションを定義 (デフォルトはOFF)
set(SYSTEMD "OFF" CACHE STRING "Enable systemd support options: OFF, USER, SYSTEM")

## SYSTEMDオプションの値を大文字に変換
string(TOUPPER ${SYSTEMD} SYSTEMD_UPPER)

## SYSTEMDオプションの値が"USER"または"SYSTEM"の場合、Systemdサービスをインストール
if(SYSTEMD_UPPER STREQUAL "USER" OR SYSTEMD_UPPER STREQUAL "SYSTEM")
    ## qnewsflash.service.qnewsflash.serviceを生成
    ## Systemdサービスファイル内の文字列を置換
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/etc/qnewsflash.service.in ${CMAKE_CURRENT_BINARY_DIR}/etc/systemd/system/qnewsflash.service @ONLY)

    ## SYSTEMD変数が"user"の場合、ホームディレクトリの取得
    if(WIN32)   # Windowsの場合
        set(USER_HOME "$ENV{USERPROFILE}")
    else()      # Linux/macOSの場合
        set(USER_HOME "$ENV{HOME}")
    endif()

    set(SYSTEMD_USER_DIR "${USER_HOME}/.config/systemd/user")

    ## SYSTEMD変数が"system"または空の場合のディレクトリ
    set(SYSTEMD_SYSTEM_DIR "/etc/systemd/system")

    ## SYSTEMD変数の値に基づいてインストールディレクトリを選択
    if(SYSTEMD_UPPER STREQUAL "USER")
        set(SYSTEMD_INSTALL_DIR ${SYSTEMD_USER_DIR})
    else()
        set(SYSTEMD_INSTALL_DIR ${SYSTEMD_SYSTEM_DIR})
    endif()

    message("qnewsflash.service は ${SYSTEMD_INSTALL_DIR} ディレクトリにインストールされます")

    ## qnewsflash.serviceファイルのインストールディレクトリを指定
    install(FILES
                ${CMAKE_CURRENT_BINARY_DIR}/etc/systemd/system/qnewsflash.service
                DESTINATION ${SYSTEMD_INSTALL_DIR}
    )
endif()

## SYSTEMDオプションの値の妥当性を確認
if(NOT SYSTEMD_UPPER STREQUAL "OFF" AND NOT SYSTEMD_UPPER STREQUAL "USER" AND NOT SYSTEMD_UPPER STREQUAL "SYSTEM")
    message(FATAL_ERROR "SYSTEMDオプションの値が無効です : '${SYSTEMD}'\n有効な値 : OFF, USER, SYSTEM")
endif()


# qNewsFlash用のラッパースクリプトのインストールルール
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Scripts/qNewsFlash.sh.in ${CMAKE_CURRENT_BINARY_DIR}/Scripts/qNewsFlash.sh @ONLY)

## qNewsFlash用のラッパースクリプトのインストールディレクトリを指定
## また、実行権限も付与する
install(PROGRAMS
        ${CMAKE_CURRENT_BINARY_DIR}/Scripts/qNewsFlash.sh
        DESTINATION ${CMAKE_INSTALL_BINDIR}
)


# 同梱しているQt5ライブラリのインストールルール
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Qt/
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/Qt
        FILES_MATCHING PATTERN "*"
)


# 実行ファイルのインストールルール
install(TARGETS qNewsFlash
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)


if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(qNewsFlash)
endif()
