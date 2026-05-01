#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

struct ProcessResult
{
    int exitCode = -1;
    QString stdoutText;
    QString stderrText;
};

class ProcessRunner
{
public:
    virtual ~ProcessRunner() = default;

    virtual ProcessResult run(const QString &program,
                              const QStringList &arguments,
                              const QProcessEnvironment &environment = QProcessEnvironment::systemEnvironment());
};
