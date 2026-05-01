#include "runtime/ProcessRunner.h"

#include <QProcess>

ProcessResult ProcessRunner::run(const QString &program,
                                 const QStringList &arguments,
                                 const QProcessEnvironment &environment)
{
    QProcess process;
    process.setProcessEnvironment(environment);
    process.start(program, arguments);

    if (!process.waitForStarted()) {
        return ProcessResult{-1, QString(), process.errorString()};
    }

    process.waitForFinished(-1);

    return ProcessResult{process.exitCode(),
                         QString::fromLocal8Bit(process.readAllStandardOutput()),
                         QString::fromLocal8Bit(process.readAllStandardError())};
}
