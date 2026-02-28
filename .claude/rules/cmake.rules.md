---
globs: ["CMakeLists.txt", "*.cmake"]
---

# CMake Rules

qNewsFlash プロジェクトの CMake ビルド規約。
このルールは `CMakeLists.txt` および `*.cmake` ファイルを編集する際に自動的に適用されます。

---

## 基本設定

### cmake_minimum_required
```cmake
cmake_minimum_required(VERSION 3.16)
```

### project
```cmake
project(qNewsFlash VERSION 1.0.0 LANGUAGES CXX)
```

### C++ 標準
```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### Qt 自動処理
```cmake
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
```

---

## 依存関係

### Qt 検出
```cmake
# Qt バージョン自動検出
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)
```

### libxml2 (pkg-config)
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)
```

### OpenSSL (Qt6用)
```cmake
if(Qt${QT_VERSION_MAJOR}_VERSION_MAJOR EQUAL 6)
    find_package(OpenSSL REQUIRED)
endif()
```

---

## ターゲット設定

### 実行ファイル
```cmake
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)
```

### インクルードパス
```cmake
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBXML2_INCLUDE_DIRS}
)
```

### リンクライブラリ
```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    ${LIBXML2_LIBRARIES}
)

# Qt6 用 OpenSSL
if(Qt${QT_VERSION_MAJOR}_VERSION_MAJOR EQUAL 6)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
    )
endif()
```

---

## ビルドオプション

### キャッシュ変数
```cmake
# 設定ファイルディレクトリ
set(SYSCONF_DIR "/etc/qNewsFlash"
    CACHE PATH "Config file directory")

# Systemd オプション
set(SYSTEMD "OFF"
    CACHE STRING "Install systemd files: OFF, USER, SYSTEM")

# カスタムライブラリパス
set(WITH_LIBXML2 ""
    CACHE PATH "Custom libxml2 pkgconfig path")
```

### オプション
```cmake
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(BUILD_TESTS "Build test suite" OFF)
```

---

## インストール

### 実行ファイル
```cmake
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
```

### 設定ファイル
```cmake
install(FILES etc/qNewsFlash.json.in
    DESTINATION ${SYSCONF_DIR}
    RENAME qNewsFlash.json
)
```

### Systemd ファイル
```cmake
if(SYSTEMD STREQUAL "USER")
    install(FILES
        ${CMAKE_BINARY_DIR}/qnewsflash.service
        ${CMAKE_BINARY_DIR}/qnewsflash.timer
        DESTINATION lib/systemd/user
    )
elseif(SYSTEMD STREQUAL "SYSTEM")
    install(FILES
        ${CMAKE_BINARY_DIR}/qnewsflash.service
        ${CMAKE_BINARY_DIR}/qnewsflash.timer
        DESTINATION lib/systemd/system
    )
endif()
```

---

## 設定ファイル生成

```cmake
configure_file(
    ${CMAKE_SOURCE_DIR}/etc/qnewsflash.service.in
    ${CMAKE_BINARY_DIR}/qnewsflash.service
    @ONLY
)
```

---

## 条件分岐

### ビルドタイプ
```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        DEBUG_MODE
    )
endif()
```

### Qt バージョン
```cmake
if(Qt${QT_VERSION_MAJOR}_VERSION_MAJOR EQUAL 6)
    # Qt6 固有の設定
else()
    # Qt5 固有の設定
endif()
```

---

## 禁止事項

### 絶対パスのハードコード
```cmake
# 禁止
target_link_libraries(${PROJECT_NAME} PRIVATE /usr/lib64/libxml2.so)

# OK: find_package/pkg-config を使用
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBXML2_LIBRARIES})
```

### 非推奨コマンド
```cmake
# 禁止: 古いコマンド
include_directories(/usr/include)
link_libraries(xml2)

# OK: ターゲット固有のコマンド
target_include_directories(${PROJECT_NAME} PRIVATE ...)
target_link_libraries(${PROJECT_NAME} PRIVATE ...)
```

---

## 推奨パターン

### ソースファイルのグループ化
```cmake
set(CORE_SOURCES
    main.cpp
    Runner.cpp
    CommandLineParser.cpp
)

set(NETWORK_SOURCES
    HtmlFetcher.cpp
    Poster.cpp
    NtpTimeFetcher.cpp
)

set(NEWS_SOURCES
    JiJiFlash.cpp
    KyodoFlash.cpp
)

set(SOURCES
    ${CORE_SOURCES}
    ${NETWORK_SOURCES}
    ${NEWS_SOURCES}
)
```

### Generator Expressions
```cmake
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)
```

---

## デバッグ

### VERBOSE 出力
```bash
make VERBOSE=1
```

### 変数確認
```cmake
message(STATUS "Qt version: ${QT_VERSION_MAJOR}")
message(STATUS "libxml2: ${LIBXML2_LIBRARIES}")
```
