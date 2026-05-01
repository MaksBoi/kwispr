#pragma once

#include "runtime/ProcessRunner.h"

#include <QString>

class LocalSttProcess
{
public:
    LocalSttProcess(QString repoRoot, ProcessRunner *runner);

    ProcessResult start(const QString &modelDir = QString());

private:
    QString binaryPath() const;
    QString catalogPath() const;

    QString m_repoRoot;
    ProcessRunner *m_runner;
};
