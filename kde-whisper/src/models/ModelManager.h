#pragma once

#include "runtime/ProcessRunner.h"

#include <QMap>
#include <QString>

class ModelManager
{
public:
    ModelManager(QString repoRoot, QString catalogPath, QString modelDir, ProcessRunner *runner = nullptr);

    static QMap<QString, bool> parseListOutput(const QString &output);

    QMap<QString, bool> listInstalledStatus();
    ProcessResult download(const QString &modelId);
    ProcessResult verify(const QString &modelId);

private:
    ProcessResult runHelper(const QStringList &commandArguments);

    QString m_repoRoot;
    QString m_catalogPath;
    QString m_modelDir;
    ProcessRunner *m_runner;
    ProcessRunner m_defaultRunner;
};
