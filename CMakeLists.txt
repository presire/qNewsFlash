cmake_minimum_required(VERSION 3.16)
project(qNewsFlash VERSION 1.1.1 LANGUAGES C CXX)


# Linuxのみビルドを許可
if(NOT (UNIX AND NOT APPLE AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "BSD"))
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


# libxml2ライブラリの設定
## WITH_LIBXML2オプションに"libxml-2.0.pc"ファイルのパスを指定する場合、任意のディレクトリに存在するlibxml2.0ライブラリを使用する
set(WITH_LIBXML2 "" CACHE STRING "Path to libxml-2.0.pc file")

## WITH_LIBXML2が空ではない場合、その値を環境変数PKG_CONFIG_PATHに追加
if(NOT WITH_LIBXML2 STREQUAL "")
    set(ENV{PKG_CONFIG_PATH} "${WITH_LIBXML2}:$ENV{PKG_CONFIG_PATH}")
endif()


## pkg-configを使用してlibxml2ライブラリを検索
pkg_search_module(LIBXML2 REQUIRED libxml-2.0)


## ライブラリのインクルードディレクトリをターゲットに追加
include_directories(${LIBXML2_INCLUDE_DIRS})


## ライブラリのリンクディレクトリをターゲットに追加
link_directories(${LIBXML2_LIBRARY_DIRS})


# OpenSSL3の設定
## WITH_OPENSSL3オプションにOpenSSL3のインストールディレクトリを指定
set(WITH_OPENSSL3 "" CACHE PATH "Path to OpenSSL3 installation directory")

## WITH_OPENSSL3が空ではない場合、環境変数を設定
if(NOT WITH_OPENSSL3 STREQUAL "")
    set(OPENSSL_ROOT_DIR "${WITH_OPENSSL3}")

    ## PATHにbinディレクトリを追加
    set(ENV{PATH} "${WITH_OPENSSL3}/bin:$ENV{PATH}")

    ## libディレクトリの存在確認
    if(EXISTS "${WITH_OPENSSL3}/lib64")
        set(OPENSSL3_LIB_DIR "lib64")
    else()
        set(OPENSSL3_LIB_DIR "lib")
    endif()

    ## LD_LIBRARY_PATHにlib / lib64を追加
    set(ENV{LD_LIBRARY_PATH} "${WITH_OPENSSL3}/${OPENSSL3_LIB_DIR}:$ENV{LD_LIBRARY_PATH}")

    ## PKG_CONFIG_PATHにlib / lib64/pkgconfigを追加
    set(ENV{PKG_CONFIG_PATH} "${WITH_OPENSSL3}/${OPENSSL3_LIB_DIR}/pkgconfig:$ENV{PKG_CONFIG_PATH}")

    ## OPENSSL_CONFに/ssl/openssl.cnfを追加
    set(ENV{OPENSSL_CONF} "${WITH_OPENSSL3}/ssl/openssl.cnf")

    ## pkg-configを使用してOpenSSL3を検索
    pkg_search_module(OPENSSL3 REQUIRED libssl libcrypto)

    if(OPENSSL3_FOUND)
        message("OpenSSL 3 is Found.")

        ## ライブラリのインクルードディレクトリをターゲットに追加
        include_directories(${OPENSSL3_INCLUDE_DIRS})

        ## ライブラリのリンクディレクトリをターゲットに追加
        link_directories(${OPENSSL3_LIBRARY_DIRS})
    endif()
else()
    find_package(OpenSSL REQUIRED)
endif()


# pkg-configを使用してQtライブラリを検索
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network)

if(QT_VERSION_MAJOR EQUAL 5)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)
elseif(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)

    # OpenSSL 3を検索
    find_package(OpenSSL 3 REQUIRED)
endif()

## Qtのpkg-configファイル
pkg_check_modules(QT_CORE     Qt${QT_VERSION_MAJOR}Core    REQUIRED IMPORTED_TARGET)
pkg_check_modules(QT_NETWORK  Qt${QT_VERSION_MAJOR}Network REQUIRED IMPORTED_TARGET)

## Qt 5.15未満はビルドエラー
if(QT_VERSION VERSION_LESS 5.15.0)
    message(FATAL_ERROR "Qt version must be greater than or equal to 5.15.0")
endif()


add_executable(qNewsFlash
        main.cpp
        Runner.h            Runner.cpp
        HtmlFetcher.h       HtmlFetcher.cpp
        Article.h           Article.cpp
        RandomGenerator.h   RandomGenerator.cpp
        Poster.h            Poster.cpp
        JiJiFlash.h         JiJiFlash.cpp
        KyodoFlash.h        KyodoFlash.cpp
        WriteMode.h         WriteMode.cpp
        CommandLineParser.h CommandLineParser.cpp
)


# プリプロセッサの定義
## デバッグビルドの場合、_DEBUGプリプロセッサを定義
target_compile_definitions(qNewsFlash PRIVATE
        $<$<CONFIG:Debug>:_DEBUG>
)

## バージョンプリプロセッサを定義 (x.y.z)
target_compile_definitions(qNewsFlash PRIVATE
    -DQNEWSFLASH_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    -DQNEWSFLASH_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    -DQNEWSFLASH_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

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


# ヘッダファイル
if(${QT_VERSION_MAJOR} EQUAL 5)
    target_include_directories(qNewsFlash PRIVATE
            ${LIBXML2_INCLUDE_DIRS}
    )
elseif(${QT_VERSION_MAJOR} EQUAL 6)
    target_include_directories(qNewsFlash PRIVATE
            ${LIBXML2_INCLUDE_DIRS}
            ${OPENSSL_INCLUDE_DIR}
    )
endif()


if(QT_VERSION_MAJOR EQUAL 5)
    target_link_libraries(qNewsFlash PRIVATE
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Network
            ${LIBXML2_LIBRARIES}
    )
elseif(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(qNewsFlash PRIVATE
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Network
            ${LIBXML2_LIBRARIES}
            OpenSSL::SSL
            OpenSSL::Crypto
    )
endif()


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
    ## qnewsflash.service, qnewsflash.timerを生成
    ## Systemdサービスファイル内の文字列を置換
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/etc/qnewsflash.service.in ${CMAKE_CURRENT_BINARY_DIR}/etc/systemd/system/qnewsflash.service @ONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/etc/qnewsflash.timer.in   ${CMAKE_CURRENT_BINARY_DIR}/etc/systemd/system/qnewsflash.timer   @ONLY)

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

    message("qnewsflash.service, qnewsflash.timer は ${SYSTEMD_INSTALL_DIR} ディレクトリにインストールされます")

    ## qnewsflash.serviceファイルのインストールディレクトリを指定
    install(FILES
                ${CMAKE_CURRENT_BINARY_DIR}/etc/systemd/system/qnewsflash.service
                ${CMAKE_CURRENT_BINARY_DIR}/etc/systemd/system/qnewsflash.timer
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


# 同梱しているQt 5 または Qt 6ライブラリのインストールルール
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Qt${QT_VERSION_MAJOR}/
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
