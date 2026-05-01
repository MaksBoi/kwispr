#include "models/ModelManager.h"

#include <QDir>

ModelManager::ModelManager(QString repoRoot, QString catalogPath, QString modelDir, ProcessRunner *runner)
    : m_repoRoot(std::move(repoRoot)),
      m_catalogPath(std::move(catalogPath)),
      m_modelDir(std::move(modelDir)),
      m_runner(runner ? runner : &m_defaultRunner)
{
}

QMap<QString, bool> ModelManager::parseListOutput(const QString &output)
{
    QMap<QString, bool> statuses;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList columns = line.split('\t');
        if (columns.size() < 2) {
            continue;
        }
        statuses.insert(columns.at(0), columns.at(1) == QStringLiteral("installed"));
    }
    return statuses;
}

QMap<QString, bool> ModelManager::listInstalledStatus()
{
    const ProcessResult result = runHelper({QStringLiteral("list")});
    if (result.exitCode != 0) {
        return {};
    }
    return parseListOutput(result.stdoutText);
}

ProcessResult ModelManager::download(const QString &modelId)
{
    return runHelper({QStringLiteral("download"), modelId});
}

ProcessResult ModelManager::verify(const QString &modelId)
{
    return runHelper({QStringLiteral("verify"), modelId});
}

ProcessResult ModelManager::runHelper(const QStringList &commandArguments)
{
    QStringList arguments;
    arguments << QDir(m_repoRoot).filePath(QStringLiteral("kwispr-models.py"))
              << QStringLiteral("--catalog") << m_catalogPath
              << QStringLiteral("--model-dir") << m_modelDir;
    arguments << commandArguments;
    return m_runner->run(QStringLiteral("python3"), arguments);
}
