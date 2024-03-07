#include <QCoreApplication>
#include <QTimer>
#include <QProcess>
#include "Runner.h"


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

#ifdef Q_OS_LINUX
    QString RunUser = "";
    QProcess Proc;
    QObject::connect(&Proc, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                     [&Proc, &RunUser]([[maybe_unused]] int exitCode, [[maybe_unused]] QProcess::ExitStatus exitStatus) {
                         RunUser = QString::fromLocal8Bit(Proc.readAllStandardOutput());
                         RunUser.replace("\n", "");
                     });
    Proc.start("whoami", QStringList({}));
    Proc.waitForFinished();

    // ランナー開始
    Runner runner(app.arguments(), RunUser);
    QTimer::singleShot(0, &runner, &Runner::run);
#else
    // ランナー開始
    Runner runner(app.arguments());
    QTimer::singleShot(0, &runner, &Runner::run);
#endif

    return app.exec();
}
