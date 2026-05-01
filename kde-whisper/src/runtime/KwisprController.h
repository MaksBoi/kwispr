#pragma once

#include "runtime/ProcessRunner.h"

#include <QString>

class KwisprController
{
public:
    KwisprController(QString repoRoot, ProcessRunner *runner);

    ProcessResult toggleRecording();
    ProcessResult retry(const QString &path);

private:
    QString scriptPath() const;

    QString m_repoRoot;
    ProcessRunner *m_runner;
};
