#include <QCoreApplication>
#include <QTimer>
#include <pwd.h>
#include <unistd.h>
#include <iostream>
#include "Runner.h"


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

#ifdef Q_OS_LINUX
    // アプリケーションを実行しているユーザ名を取得
    QString RunUser = "";
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        RunUser = QString::fromLocal8Bit(pw->pw_name);
    }

    if (RunUser.isEmpty()) std::cerr << QString("エラー : 実行ユーザ名の取得に失敗しました").toStdString() << std::endl;

    // ランナーのインスタンス生成
    Runner runner(QCoreApplication::arguments(), RunUser);
#elif Q_OS_WIN
    // ランナーのインスタンス生成
    Runner runner(QCoreApplication::arguments());
#endif

    // ランナー開始
    QTimer::singleShot(0, &runner, &Runner::run);

    return app.exec();
}
